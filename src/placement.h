#pragma once

#include <QByteArray>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

// One monitor as the compositor lays it out, in logical pixels.
struct ScreenInfo {
    QString name;
    QRect rect;
    bool focused = false;
};

// Reading Hyprland's JSON and cursor output is the only part of placing the
// card that can be tested without a desktop, so it lives on its own.
QPoint parseCursorPosition(const QByteArray &output, bool *ok = nullptr);
QList<ScreenInfo> parseMonitors(const QByteArray &json);
QRect parseActiveWindow(const QByteArray &json);
QString parseActiveWindowClass(const QByteArray &json);
int screenIndexAt(const QPoint &point, const QList<ScreenInfo> &screens);
QPoint chooseAnchor(const QPoint &cursor, const QList<ScreenInfo> &screens,
                    const QRect &activeWindow, bool *ok = nullptr);
// Where a card of this size sits when its anchor is at `anchor`, both in the
// same screen's coordinates. Flips left near the right edge and above near the
// bottom, and never lands under the bar at the top.
QPoint popoverPosition(const QPoint &anchor, const QSize &popover, const QSize &screen,
                       int margin = 12);
