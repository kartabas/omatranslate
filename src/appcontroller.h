#pragma once

#include <QJsonObject>
#include <QObject>
#include <QPointer>

class Backend;
class QQuickWindow;
class TranslationService;

// Owns what a second launch and the bar plugin may ask the running app to do:
// show the window, open Settings, translate the current highlight, switch the
// background service on or off, and quit. Also translates those requests into
// the main window's visible state.
class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool serviceMode READ serviceMode CONSTANT)
    Q_PROPERTY(bool mainVisible READ mainVisible WRITE setMainVisible NOTIFY mainVisibleChanged)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    AppController(Backend *backend, TranslationService *translate, bool serviceMode,
                  QObject *parent = nullptr);

    bool serviceMode() const { return m_serviceMode; }
    bool mainVisible() const { return m_mainVisible; }
    void setMainVisible(bool visible);
    QString version() const;

    void setWindow(QQuickWindow *window);

    // The control channel's handler. Returns the reply for one command.
    QJsonObject handle(const QString &command, const QJsonObject &arguments);

    Q_INVOKABLE void showMain();
    Q_INVOKABLE void hideMain();
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void requestsQuit();

signals:
    void mainVisibleChanged();
    void settingsRequested();

private:
    void quitNow();
    void quitSoon();

    QPointer<Backend> m_backend;
    QPointer<TranslationService> m_translate;
    QPointer<QQuickWindow> m_window;
    bool m_serviceMode = false;
    bool m_mainVisible = true;
};
