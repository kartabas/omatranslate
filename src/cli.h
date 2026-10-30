#pragma once

#include <QString>
#include <QStringList>

// The commands that need no display, so a script or a coding agent can
// translate without opening the window.
bool isCliCommand(const QString &word);
QString cliSummary();
int runCli(const QStringList &arguments);
