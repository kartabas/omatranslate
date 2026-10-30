#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

// Reads the Wayland primary selection, where the text lives that the person
// highlighted in their PDF reader. Qt's QClipboard has no primary selection on
// Wayland, so this asks wl-paste for it and reports only real changes.
class SelectionWatcher : public QObject {
    Q_OBJECT

public:
    explicit SelectionWatcher(const QString &command = {}, int interval = 500, QObject *parent = nullptr);
    ~SelectionWatcher() override;

    void start();
    void stop();
    QString text() const { return m_text; }
    QString error() const { return m_error; }

    static QString defaultCommand();

signals:
    void textChanged(const QString &text);

private:
    void poll();
    void read();

    QString m_command;
    int m_interval;
    QTimer m_timer;
    QProcess m_process;
    QString m_text;
    QString m_error;
    bool m_running = false;
};
