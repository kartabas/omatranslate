#pragma once

#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>

class Backend;
class QQmlEngine;
class QQuickWindow;
class QScreen;

namespace LayerShellQt {
class Window;
}

// A small card that floats over the PDF reader, next to the passage being
// read. The reader is never modified: the card is a layer-shell surface in the
// top layer, so it sits above Evince without taking focus away from it.
class Overlay : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(bool card READ card NOTIFY changed)

public:
    explicit Overlay(Backend *backend, QObject *parent = nullptr);
    ~Overlay() override;

    // Loads the card's window into the app's engine. Returns false when the
    // window cannot be created, in which case the app still works.
    bool attach(QQmlEngine *engine);

    QString mode() const { return m_mode; }
    bool card() const { return m_mode == QLatin1String("card"); }

    // Shows the card and translates the passage that is currently highlighted.
    Q_INVOKABLE void translate();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void dismiss();

signals:
    void changed();

private:
    struct Placement {
        QPoint anchor;             // global, in logical pixels
        QRect screen;              // the monitor's geometry, same coordinates
        QPointer<QScreen> device;
        QString activeClass;       // the window the highlight came from
        bool found = false;
    };

    void onBackendChanged();
    void onHighlighted(const QString &text);
    void setMode(const QString &mode);
    void showWindow();
    void place();
    Placement locate() const;

    Backend *m_backend = nullptr;
    QQuickWindow *m_window = nullptr;
    LayerShellQt::Window *m_layer = nullptr;
    QString m_mode = QStringLiteral("hidden");
    Placement m_placement;
    QString m_lastSelection;
    QString m_hyprctl;
};
