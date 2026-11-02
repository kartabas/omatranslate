#include "appcontroller.h"

#include <QCoreApplication>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "translationservice.h"

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

void AppController::showMain()
{
    setMainVisible(true);
    if (m_window) {
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }
}

void AppController::hideMain()
{
    setMainVisible(false);
}

void AppController::openSettings()
{
    showMain();
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
        showMain();
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
