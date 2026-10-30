#include "selectionwatcher.h"

#include <QStandardPaths>

namespace {
// A selection is never this long; anything bigger is a runaway copy or a
// binary blob, and sending it to a translation service would be rude.
constexpr int MaximumSelection = 200000;
}

SelectionWatcher::SelectionWatcher(const QString &command, int interval, QObject *parent)
    : QObject(parent), m_command(command), m_interval(interval) {
    if (m_command.isEmpty())
        m_command = defaultCommand();
    if (m_command.isEmpty())
        m_error = tr("Watching the selection needs wl-clipboard. Install it with omarchy-pkg-add wl-clipboard.");
    m_timer.setInterval(m_interval);
    connect(&m_timer, &QTimer::timeout, this, &SelectionWatcher::poll);
    connect(&m_process, &QProcess::finished, this, &SelectionWatcher::read);
}

SelectionWatcher::~SelectionWatcher() {
    stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QString SelectionWatcher::defaultCommand() {
    const QString wlPaste = QStandardPaths::findExecutable("wl-paste");
    if (wlPaste.isEmpty())
        return {};
    return wlPaste + QStringLiteral(" --primary --no-newline --type text");
}

void SelectionWatcher::start() {
    if (m_running || m_command.isEmpty())
        return;
    m_running = true;
    m_timer.start();
    poll();
}

void SelectionWatcher::stop() {
    m_running = false;
    m_timer.stop();
}

void SelectionWatcher::poll() {
    // One wl-paste at a time: it answers at once, but a compositor under load
    // can take longer than the interval, and stacking processes helps nobody.
    if (!m_running || m_process.state() != QProcess::NotRunning)
        return;
    m_process.startCommand(m_command);
}

void SelectionWatcher::read() {
    if (m_process.exitStatus() != QProcess::NormalExit || m_process.exitCode() != 0)
        return;
    QString text = QString::fromUtf8(m_process.readAllStandardOutput());
    if (text.size() > MaximumSelection)
        text.truncate(MaximumSelection);
    if (text == m_text)
        return;
    m_text = text;
    emit textChanged(m_text);
}
