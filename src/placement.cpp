#include "placement.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>

QPoint parseCursorPosition(const QByteArray &output, bool *ok)
{
    if (ok)
        *ok = false;
    static const QRegularExpression pattern(QStringLiteral("^\\s*(-?\\d+)\\s*,\\s*(-?\\d+)\\s*$"));
    const QString text = QString::fromUtf8(output).trimmed();
    const auto match = pattern.match(text);
    if (!match.hasMatch())
        return {};
    if (ok)
        *ok = true;
    return QPoint(match.captured(1).toInt(), match.captured(2).toInt());
}

QList<ScreenInfo> parseMonitors(const QByteArray &json)
{
    QList<ScreenInfo> screens;
    const QJsonArray array = QJsonDocument::fromJson(json).array();
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        const QString name = object.value(QStringLiteral("name")).toString();
        const double scale = object.value(QStringLiteral("scale")).toDouble(1.0);
        const int width = object.value(QStringLiteral("width")).toInt();
        const int height = object.value(QStringLiteral("height")).toInt();
        if (name.isEmpty() || width <= 0 || height <= 0 || scale <= 0)
            continue;
        const QRect rect(qRound(object.value(QStringLiteral("x")).toDouble()),
                         qRound(object.value(QStringLiteral("y")).toDouble()),
                         qRound(width / scale), qRound(height / scale));
        screens.append({name, rect, object.value(QStringLiteral("focused")).toBool()});
    }
    return screens;
}

QRect parseActiveWindow(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    const QJsonArray at = object.value(QStringLiteral("at")).toArray();
    const QJsonArray size = object.value(QStringLiteral("size")).toArray();
    if (at.size() != 2 || size.size() != 2)
        return {};
    const QRect rect(at.at(0).toInt(), at.at(1).toInt(), size.at(0).toInt(), size.at(1).toInt());
    return rect.isValid() ? rect : QRect();
}

QString parseActiveWindowClass(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object().value(QStringLiteral("class")).toString();
}

int screenIndexAt(const QPoint &point, const QList<ScreenInfo> &screens)
{
    for (int i = 0; i < screens.size(); ++i)
        if (screens.at(i).rect.contains(point))
            return i;
    return -1;
}

QPoint chooseAnchor(const QPoint &cursor, const QList<ScreenInfo> &screens,
                    const QRect &activeWindow, bool *ok)
{
    if (ok)
        *ok = false;
    if (screens.isEmpty())
        return {};

    // With a mouse selection the pointer is still on the passage. A keyboard
    // selection leaves it wherever it was, so the active window is the better
    // place; its middle is over the page the person is reading.
    QPoint anchor = cursor;
    if (activeWindow.isValid() && !activeWindow.contains(cursor))
        anchor = activeWindow.center();
    if (screenIndexAt(anchor, screens) < 0) {
        for (const auto &screen : screens) {
            if (screen.focused) {
                anchor = screen.rect.center();
                break;
            }
        }
    }
    if (screenIndexAt(anchor, screens) < 0)
        return {};
    if (ok)
        *ok = true;
    return anchor;
}

QPoint popoverPosition(const QPoint &anchor, const QSize &popover, const QSize &screen, int margin)
{
    int x = anchor.x() + 16;
    if (x + popover.width() > screen.width() - margin)
        x = anchor.x() - popover.width() - 16;
    x = qBound(margin, x, qMax(margin, screen.width() - popover.width() - margin));

    // Below the pointer reads like a tooltip; flip above it near the bottom.
    int y = anchor.y() + 20;
    if (y + popover.height() > screen.height() - margin)
        y = anchor.y() - popover.height() - 12;
    // Stay clear of the 22 px bar at the top.
    y = qBound(margin + 28, y, qMax(margin + 28, screen.height() - popover.height() - margin));
    return {x, y};
}

QRect centeredWindowGeometry(const QRect &screen, const QSize &window)
{
    const QSize size = window.boundedTo(screen.size());
    return QRect(screen.left() + (screen.width() - size.width()) / 2,
                 screen.top() + (screen.height() - size.height()) / 2,
                 size.width(), size.height());
}

QStringList mainWindowPlacementCommands(int pid, bool onTop)
{
    const QString window = QStringLiteral("pid:%1").arg(pid);
    QStringList commands;
    // Hyprland can only center a floating window, so float it first. Pinning
    // keeps it above other windows even after they take focus.
    commands << QStringLiteral("dispatch hl.dsp.window.float({action='on',window='%1'})").arg(window)
             << QStringLiteral("dispatch hl.dsp.window.center({window='%1'})").arg(window);
    if (onTop) {
        commands << QStringLiteral("dispatch hl.dsp.window.pin({action='on',window='%1'})").arg(window)
                 << QStringLiteral("dispatch hl.dsp.window.alter_zorder({mode='top',window='%1'})")
                        .arg(window);
    }
    return commands;
}

QStringList mainWindowUnpinCommands(int pid)
{
    return {QStringLiteral("dispatch hl.dsp.window.pin({action='off',window='pid:%1'})").arg(pid)};
}
