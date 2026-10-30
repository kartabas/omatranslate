#include "overlay.h"

#include <QGuiApplication>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>

#include <LayerShellQt/Window>

#include "backend.h"
#include "placement.h"

namespace {

QByteArray ask(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(300))
        return {};
    if (!process.waitForFinished(500)) {
        process.kill();
        process.waitForFinished(200);
        return {};
    }
    return process.readAllStandardOutput();
}

QScreen *screenNamed(const QString &name)
{
    for (QScreen *screen : QGuiApplication::screens())
        if (screen->name() == name)
            return screen;
    return nullptr;
}

bool isOwnWindow(const QString &windowClass)
{
    return windowClass.contains(QLatin1String("omatranslate"), Qt::CaseInsensitive);
}

}

Overlay::Overlay(Backend *backend, QObject *parent) : QObject(parent), m_backend(backend)
{
    m_hyprctl = QStandardPaths::findExecutable(QStringLiteral("hyprctl"));

    // A chip that nobody clicks should not sit on the page forever.
    m_chipTimer.setSingleShot(true);
    m_chipTimer.setInterval(8000);
    connect(&m_chipTimer, &QTimer::timeout, this, [this] {
        if (m_mode == QLatin1String("chip"))
            dismiss();
    });

    if (m_backend) {
        connect(m_backend, &Backend::highlighted, this, &Overlay::onHighlighted);
        connect(m_backend, &Backend::changed, this, &Overlay::onBackendChanged);
    }
}

Overlay::~Overlay()
{
    if (m_window && m_layer)
        m_window->hide();
    delete m_window;
}

bool Overlay::attach(QQmlEngine *engine)
{
    if (!engine || m_window)
        return m_window != nullptr;

    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/TranslationOverlay.qml")));
    QObject *object = component.create();
    if (!object) {
        for (const auto &error : component.errors())
            qWarning("omatranslate: %s", qPrintable(error.toString()));
        return false;
    }

    m_window = qobject_cast<QQuickWindow *>(object);
    if (!m_window) {
        delete object;
        return false;
    }
    m_window->setTitle(QStringLiteral("Omatranslate"));

    // The layer-shell surface is what lets the card hover above the reader
    // instead of becoming another window in the stack. It must be configured
    // before the window is first shown.
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        m_layer = LayerShellQt::Window::get(m_window);
        if (m_layer) {
            m_layer->setLayer(LayerShellQt::Window::LayerOverlay);
            m_layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
                                | LayerShellQt::Window::AnchorLeft);
            m_layer->setExclusiveZone(-1);
            m_layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
            m_layer->setScope(QStringLiteral("omatranslate"));
        }
    }
    if (!m_layer) {
        // No layer shell (an X11 session, or another compositor): a normal
        // frameless window that stays on top and keeps the focus where it is.
        m_window->setFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus
                           | Qt::WindowStaysOnTopHint);
    }

    connect(m_window, &QQuickWindow::widthChanged, this, [this] { place(); });
    connect(m_window, &QQuickWindow::heightChanged, this, [this] { place(); });
    return true;
}

void Overlay::translate()
{
    if (!m_backend || !m_window)
        return;
    if (m_backend->selection().isEmpty()) {
        m_backend->translateNow();
        return;
    }
    // The chip was shown where the highlight is. Using that anchor again keeps
    // the card from jumping when the pointer has moved to the chip.
    if (!m_placement.found)
        m_placement = locate();
    setMode(QStringLiteral("card"));
    showWindow();
    m_backend->translateNow();
}

void Overlay::copy()
{
    if (m_backend)
        m_backend->copyTranslation();
}

void Overlay::dismiss()
{
    m_chipTimer.stop();
    if (m_window)
        m_window->hide();
    setMode(QStringLiteral("hidden"));
}

void Overlay::onHighlighted(const QString &text)
{
    Q_UNUSED(text)
    if (!m_window)
        return;
    m_placement = locate();

    // Highlighting text inside Omatranslate itself is how you copy a
    // translation; a Translate chip over our own window would be in the way.
    if (isOwnWindow(m_placement.activeClass))
        return;
    // Without a compositor that can say where the cursor is, the chip would
    // guess. The main window and Enter still translate.
    if (!m_placement.found)
        return;

    if (m_backend && m_backend->autoTranslate()) {
        setMode(QStringLiteral("card"));
        showWindow();
    } else {
        setMode(QStringLiteral("chip"));
        showWindow();
    }
}

void Overlay::onBackendChanged()
{
    if (!m_backend || !m_window)
        return;

    const QString selection = m_backend->selection();
    m_lastSelection = selection;

    if (selection.isEmpty()) {
        if (m_mode != QLatin1String("hidden"))
            dismiss();
        return;
    }

    // Enter in the main window, the top-bar Translate button, or the automatic
    // switch: once a translation starts, the card is what should be visible.
    if (m_backend->status() == QLatin1String("translating")
        && m_mode != QLatin1String("card")) {
        setMode(QStringLiteral("card"));
        showWindow();
    }
}

void Overlay::setMode(const QString &mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    if (m_mode == QLatin1String("chip"))
        m_chipTimer.start();
    else
        m_chipTimer.stop();
    emit changed();
}

void Overlay::showWindow()
{
    if (!m_window)
        return;
    place();
    if (!m_window->isVisible())
        m_window->show();
    // Content height settles a moment after the mode changes; the size
    // signals then place it again with the final height.
    place();
}

void Overlay::place()
{
    if (!m_window || !m_window->isVisible() || m_window->size().isEmpty())
        return;

    QRect screen = m_placement.screen;
    if (!screen.isValid() && m_window->screen())
        screen = m_window->screen()->geometry();
    if (!screen.isValid())
        return;

    const QSize size = m_window->size();
    int x = 0;
    int y = 0;
    if (m_placement.found) {
        const QPoint local = m_placement.anchor - screen.topLeft();
        const QPoint position = popoverPosition(local, size, screen.size());
        x = position.x();
        y = position.y();
    } else {
        // Somewhere calm and out of the way when the pointer is unknown.
        x = (screen.width() - size.width()) / 2;
        y = 70;
    }

    if (m_layer) {
        if (m_placement.device)
            m_layer->setScreen(m_placement.device);
        m_layer->setMargins(QMargins(x, y, 0, 0));
    } else {
        m_window->setPosition(screen.topLeft() + QPoint(x, y));
    }
}

Overlay::Placement Overlay::locate() const
{
    Placement placement;

    if (!m_hyprctl.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
        const QList<ScreenInfo> screens = parseMonitors(ask(m_hyprctl, {QStringLiteral("monitors"),
                                                                        QStringLiteral("-j")}));
        if (!screens.isEmpty()) {
            bool cursorKnown = false;
            const QPoint cursor = parseCursorPosition(ask(m_hyprctl, {QStringLiteral("cursorpos")}),
                                                      &cursorKnown);
            const QByteArray active = ask(m_hyprctl, {QStringLiteral("activewindow"),
                                                      QStringLiteral("-j")});
            placement.activeClass = parseActiveWindowClass(active);
            const QRect activeWindow = parseActiveWindow(active);

            bool anchored = false;
            const QPoint anchor = chooseAnchor(
                cursorKnown ? cursor : QPoint(-1000000, -1000000), screens, activeWindow, &anchored);
            if (anchored) {
                const int index = screenIndexAt(anchor, screens);
                if (index >= 0) {
                    placement.anchor = anchor;
                    placement.screen = screens.at(index).rect;
                    placement.device = screenNamed(screens.at(index).name);
                    if (!placement.device)
                        placement.device = QGuiApplication::screenAt(anchor);
                    placement.found = true;
                }
            }
        }
    }

    if (!placement.screen.isValid()) {
        if (QScreen *primary = QGuiApplication::primaryScreen()) {
            placement.screen = primary->geometry();
            placement.anchor = placement.screen.center();
            placement.device = primary;
        }
    }
    return placement;
}
