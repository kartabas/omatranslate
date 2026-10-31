#pragma once

#include <QHash>
#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <functional>

class QLocalSocket;

// The one channel every other part of the app uses to find a running
// Omatranslate: the bar plugin, `omatranslate status`, and a second launch.
// One JSON object goes down the socket and one comes back.
class ControlServer : public QObject {
    Q_OBJECT

public:
    using Handler = std::function<QJsonObject(const QString &command, const QJsonObject &arguments)>;

    explicit ControlServer(QObject *parent = nullptr);

    // A stable name per user, so two people on one machine never collide.
    static QString socketName();

    // True when another Omatranslate is already answering on the socket.
    static bool isRunning(int timeout = 400);

    // Sends one command. `connected` says whether a process answered at all;
    // an empty reply with connected true means it answered with nothing.
    static QJsonObject send(const QString &command, const QJsonObject &arguments = {},
                            int timeout = 3000, bool *connected = nullptr);

    void setHandler(Handler handler);
    bool listen();
    void close();

private:
    void accept();
    void read(QLocalSocket *socket);
    void reply(QLocalSocket *socket, const QByteArray &line);

    QLocalServer m_server;
    QHash<QLocalSocket *, QByteArray> m_buffers;
    Handler m_handler;
};
