#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

// The desktop's colors and window shape, read from Omarchy's current theme and
// re-read whenever it changes, so a theme switch repaints the app in place.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString darkBackground READ darkBackground NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString danger READ danger NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QString background() const { return m_background; }
    QString darkBackground() const { return m_darkBackground; }
    QString foreground() const { return m_foreground; }
    QString muted() const { return m_muted; }
    QString selection() const { return m_selection; }
    QString danger() const { return m_danger; }
    int radius() const { return m_radius; }

    static QString readAccent(const QString &path);
    static QString readColor(const QString &path, const QString &key, const QString &fallback);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QString m_accent = "#FFD60A";
    QString m_background = "#1e1e2e";
    QString m_darkBackground = "#161622";
    QString m_foreground = "#cdd6f4";
    QString m_muted = "#7a7a86";
    QString m_selection = "#45475a";
    QString m_danger = "#f38ba8";
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
