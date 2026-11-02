#include "controlserver.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QProcessEnvironment>
#include <QThread>
#include <QTimer>
#include <utility>

namespace {

QByteArray requestLine(const QString &command, const QJsonObject &arguments)
{
    QJsonObject request;
    request.insert(QStringLiteral("command"), command);
    if (!arguments.isEmpty())
        request.insert(QStringLiteral("arguments"), arguments);
    return QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n';
}

}

ControlServer::ControlServer(QObject *parent) : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, &ControlServer::accept);
}

QString ControlServer::socketName()
{
    // A name per user: two people sharing a machine each get their own
    // running app instead of one stealing the other's socket.
    QString name = QStringLiteral("omatranslate-") + qEnvironmentVariable("USER", QStringLiteral("user"));
    name.replace(QLatin1Char('/'), QLatin1Char('-'));
    name.replace(QLatin1Char('\\'), QLatin1Char('-'));
    return name;
}

bool ControlServer::isRunning(int timeout)
{
    bool connected = false;
    const QJsonObject reply = send(QStringLiteral("status"), {}, timeout, &connected);
    return connected && reply.value(QStringLiteral("ok")).toBool();
}

QJsonObject ControlServer::send(const QString &command, const QJsonObject &arguments,
                                int timeout, bool *connected)
{
    if (connected)
        *connected = false;

    QLocalSocket socket;
    socket.connectToServer(socketName());
    if (!socket.waitForConnected(timeout))
        return {};

    if (connected)
        *connected = true;

    socket.write(requestLine(command, arguments));
    socket.flush();

    QByteArray data;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(timeout);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::readyRead, &loop, [&] {
        data += socket.readAll();
        if (data.contains('\n'))
            loop.quit();
    });
    QObject::connect(&socket, &QLocalSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&socket, &QLocalSocket::errorOccurred, &loop, [&loop](QLocalSocket::LocalSocketError) {
        loop.quit();
    });
    timer.start();
    if (!data.contains('\n'))
        loop.exec();

    const int newline = data.indexOf('\n');
    if (newline < 0)
        return {};
    return QJsonDocument::fromJson(data.left(newline)).object();
}

void ControlServer::setHandler(Handler handler)
{
    m_handler = std::move(handler);
}

bool ControlServer::startService(bool watch)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(QStringLiteral("QT_QPA_PLATFORM"));
    environment.remove(QStringLiteral("QT_QPA_PLATFORMTHEME"));
    process.setProcessEnvironment(environment);
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setArguments(watch ? QStringList{QStringLiteral("service"), QStringLiteral("--enable")}
                               : QStringList{QStringLiteral("service")});
    return process.startDetached();
}

bool ControlServer::waitForService(int timeout)
{
    for (int waited = 0; waited < timeout; waited += 100) {
        if (isRunning(300))
            return true;
        QThread::msleep(100);
    }
    return isRunning(300);
}

bool ControlServer::listen()
{
    close();
    // A live app already answers on this name. Qt would take the socket over
    // and leave that copy unreachable, so never even try.
    if (isRunning(500))
        return false;
    // A socket left behind by a crash keeps the name reserved; nobody answers
    // on it, so removing it is safe.
    QLocalServer::removeServer(socketName());
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(socketName()))
        return false;
    // Two copies can still race here. Only the one whose status names this
    // process keeps the socket; the other leaves it alone.
    bool connected = false;
    const QJsonObject reply = send(QStringLiteral("status"), {}, 500, &connected);
    if (connected && reply.value(QStringLiteral("pid")).toInt() != QCoreApplication::applicationPid()) {
        m_server.close();
        return false;
    }
    return true;
}

void ControlServer::close()
{
    const auto clients = m_buffers.keys();
    for (QLocalSocket *socket : clients)
        socket->disconnectFromServer();
    m_buffers.clear();
    if (m_server.isListening())
        m_server.close();
}

void ControlServer::accept()
{
    while (QLocalSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { read(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            m_buffers.remove(socket);
            socket->deleteLater();
        });
    }
}

void ControlServer::read(QLocalSocket *socket)
{
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();
    const int newline = buffer.indexOf('\n');
    if (newline < 0)
        return;
    const QByteArray line = buffer.left(newline);
    m_buffers.remove(socket);
    reply(socket, line);
}

void ControlServer::reply(QLocalSocket *socket, const QByteArray &line)
{
    QJsonObject response;
    const QJsonObject request = QJsonDocument::fromJson(line).object();
    const QString command = request.value(QStringLiteral("command")).toString();
    const QJsonObject arguments = request.value(QStringLiteral("arguments")).toObject();
    if (command.isEmpty()) {
        response.insert(QStringLiteral("ok"), false);
        response.insert(QStringLiteral("error"), QStringLiteral("No command."));
    } else if (m_handler) {
        response = m_handler(command, arguments);
    } else {
        response.insert(QStringLiteral("ok"), false);
        response.insert(QStringLiteral("error"), QStringLiteral("Not ready."));
    }
    socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n');
    socket->flush();
    socket->disconnectFromServer();
}
