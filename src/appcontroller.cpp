#include "appcontroller.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "placement.h"
#include "translationservice.h"

namespace {

bool hyprctlAvailable()
{
    return QGuiApplication::platformName().startsWith(QLatin1String("wayland"))
           && !QStandardPaths::findExecutable(QStringLiteral("hyprctl")).isEmpty()
           && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE");
}

void runHyprctl(const QStringList &commands)
{
    const QString hyprctl = QStandardPaths::findExecutable(QStringLiteral("hyprctl"));
    QProcess process;
    process.start(hyprctl, {QStringLiteral("--batch"), commands.join(QStringLiteral("; "))});
    if (process.waitForStarted(300) && process.waitForFinished(500))
        return;
    process.kill();
    process.waitForFinished(200);
}

} // namespace

AppController::AppController(Backend *backend, TranslationService *translate, bool serviceMode,
                             QObject *parent)
    : QObject(parent), m_backend(backend), m_translate(translate), m_serviceMode(serviceMode),
      m_mainVisible(!serviceMode)
{
}

QString AppController::version() const
{
    return QCoreApplication::applicationVersion();
}

void AppController::setWindow(QQuickWindow *window)
{
    m_window = window;
}

void AppController::setMainVisible(bool visible)
{
    if (m_mainVisible == visible)
        return;
    m_mainVisible = visible;
    if (m_window)
        m_window->setVisible(visible);
    emit mainVisibleChanged();
}

void AppController::showMain(bool onTop)
{
    setMainVisible(true);
    if (!m_window) {
        m_onTopApplied = false;
        return;
    }
    m_window->show();
    m_window->raise();
    m_window->requestActivate();

    if (!onTop)
        return;
    // Give the compositor a moment to map the surface before asking Hyprland
    // to find and float it. The commands are idempotent, so a second Open app
    // click simply re-centers the window.
    QTimer::singleShot(80, this, [this] {
        if (m_mainVisible)
            placeMainOnTop();
    });
}

void AppController::placeMainOnTop()
{
    if (!m_window)
        return;

    if (hyprctlAvailable()) {
        runHyprctl(mainWindowPlacementCommands(QCoreApplication::applicationPid(), true));
    } else {
        // X11 (and any other compositor) can place a window itself; on
        // Wayland without Hyprland the best we can do is ask Qt for on-top.
        if (!(m_window->flags() & Qt::WindowStaysOnTopHint)) {
            m_window->setFlags(m_window->flags() | Qt::WindowStaysOnTopHint);
            m_window->show();
        }
        QScreen *screen = m_window->screen() ? m_window->screen() : QGuiApplication::primaryScreen();
        if (screen)
            m_window->setGeometry(centeredWindowGeometry(screen->availableGeometry(), m_window->size()));
    }
    m_onTopApplied = true;
}

void AppController::clearOnTop()
{
    if (!m_onTopApplied)
        return;
    if (hyprctlAvailable()) {
        runHyprctl(mainWindowUnpinCommands(QCoreApplication::applicationPid()));
    } else if (m_window && (m_window->flags() & Qt::WindowStaysOnTopHint)) {
        m_window->setFlags(m_window->flags() & ~Qt::WindowStaysOnTopHint);
    }
    m_onTopApplied = false;
}

void AppController::hideMain()
{
    clearOnTop();
    setMainVisible(false);
}

void AppController::openSettings()
{
    // The bar's Settings action is the same front door as Open app: bring the
    // window up centered and above the reader, then show the sheet.
    showMain(true);
    // The popup lives in QML; let the window settle first so it opens on top.
    QTimer::singleShot(0, this, [this] { emit settingsRequested(); });
}

void AppController::requestsQuit()
{
    quitNow();
}

void AppController::quitSoon()
{
    // Answer on the socket before the process disappears.
    QTimer::singleShot(0, this, [this] { quitNow(); });
}

void AppController::quitNow()
{
    clearOnTop();
    // Hide first: a visible Quick window keeps the process waiting on a render
    // thread that no longer has a screen. exit() rather than quit(), because
    // quit() is swallowed once the window has been shown.
    if (m_window)
        m_window->hide();
    QCoreApplication::exit(0);
}

QJsonObject AppController::handle(const QString &command, const QJsonObject &arguments)
{
    QJsonObject reply;
    reply.insert(QStringLiteral("ok"), true);

    if (command == QLatin1String("status")) {
        reply.insert(QStringLiteral("running"), true);
        reply.insert(QStringLiteral("service"), m_serviceMode);
        reply.insert(QStringLiteral("pid"), QCoreApplication::applicationPid());
        reply.insert(QStringLiteral("version"), version());
        reply.insert(QStringLiteral("enabled"), m_backend && m_backend->enabled());
        reply.insert(QStringLiteral("status"), m_backend ? m_backend->status() : QString());
        reply.insert(QStringLiteral("historyCount"), m_backend ? m_backend->historyCount() : 0);
        reply.insert(QStringLiteral("document"), m_backend ? m_backend->documentName() : QString());
        reply.insert(QStringLiteral("targetLanguage"), m_backend ? m_backend->targetLanguage() : QString());
        reply.insert(QStringLiteral("provider"), m_translate ? m_translate->provider() : QString());
        reply.insert(QStringLiteral("providerName"), m_translate ? m_translate->providerName() : QString());
        reply.insert(QStringLiteral("endpoint"), m_translate ? m_translate->endpoint() : QString());
        return reply;
    }

    if (command == QLatin1String("show")) {
        showMain();
        return reply;
    }

    if (command == QLatin1String("hide")) {
        hideMain();
        return reply;
    }

    if (command == QLatin1String("settings")) {
        openSettings();
        return reply;
    }

    if (command == QLatin1String("open")) {
        const QString path = arguments.value(QStringLiteral("path")).toString();
        bool opened = true;
        if (!path.isEmpty() && m_backend)
            opened = m_backend->openUrl(QUrl::fromUserInput(path));
        // This is what the bar's Open app button sends: the window comes up
        // centered and above the reader instead of wherever the compositor put it.
        showMain(true);
        reply.insert(QStringLiteral("opened"), opened);
        return reply;
    }

    if (command == QLatin1String("translate")) {
        if (m_backend)
            m_backend->translateNow();
        return reply;
    }

    if (command == QLatin1String("enable")) {
        if (m_backend)
            m_backend->setEnabled(true);
        reply.insert(QStringLiteral("enabled"), true);
        return reply;
    }

    if (command == QLatin1String("disable")) {
        if (m_backend)
            m_backend->setEnabled(false);
        reply.insert(QStringLiteral("enabled"), false);
        reply.insert(QStringLiteral("keptOpen"), m_backend && m_backend->unsaved());
        return reply;
    }

    if (command == QLatin1String("stop")) {
        if (m_backend)
            m_backend->setEnabled(false);
        const bool keptOpen = m_backend && m_backend->unsaved();
        reply.insert(QStringLiteral("enabled"), false);
        reply.insert(QStringLiteral("stopped"), !keptOpen);
        reply.insert(QStringLiteral("keptOpen"), keptOpen);
        if (!keptOpen)
            quitSoon();
        return reply;
    }

    if (command == QLatin1String("quit")) {
        reply.insert(QStringLiteral("stopped"), true);
        reply.insert(QStringLiteral("enabled"), m_backend && m_backend->enabled());
        quitSoon();
        return reply;
    }

    reply.insert(QStringLiteral("ok"), false);
    reply.insert(QStringLiteral("error"), QStringLiteral("Unknown command %1.").arg(command));
    return reply;
}
