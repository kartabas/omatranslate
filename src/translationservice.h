#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantList>

class TranslationProvider;

// Owns the translation service picked in Settings and builds the right client
// for it. Everything above this class only sees translate(), finished() and
// failed(), whichever provider is underneath.
class TranslationService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString provider READ provider NOTIFY changed)
    Q_PROPERTY(QString providerName READ providerName NOTIFY changed)
    Q_PROPERTY(QString endpoint READ endpoint NOTIFY changed)
    Q_PROPERTY(QString disclosure READ disclosure NOTIFY changed)
    Q_PROPERTY(QString deepLKey READ deepLKey NOTIFY changed)
    Q_PROPERTY(QString libreUrl READ libreUrl NOTIFY changed)
    Q_PROPERTY(QString libreKey READ libreKey NOTIFY changed)
    Q_PROPERTY(QString settingsPath READ settingsPath NOTIFY changed)

public:
    explicit TranslationService(QSettings *settings = nullptr, QObject *parent = nullptr);
    ~TranslationService() override;

    // Virtual so the tests can answer without a network.
    virtual void translate(const QString &text, const QString &from, const QString &to);
    virtual void cancel();

    QString provider() const { return m_providerId; }
    QString providerName() const;
    QString endpoint() const;
    QString disclosure() const;
    QString deepLKey() const;
    QString libreUrl() const;
    QString libreKey() const;
    QString settingsPath() const;

    // The choices Settings offers: id, name and a one-line description.
    Q_INVOKABLE QVariantList providers() const;
    Q_INVOKABLE QString endpointFor(const QString &id) const;

    Q_INVOKABLE void setProvider(const QString &id);
    Q_INVOKABLE void setDeepLKey(const QString &key);
    Q_INVOKABLE void setLibreUrl(const QString &url);
    Q_INVOKABLE void setLibreKey(const QString &key);

signals:
    void finished(const QString &translation, const QString &detected);
    void failed(const QString &message);
    void changed();

private:
    void rebuild();
    void saveSetting(const QString &key, const QVariant &value);
    QString readString(const QString &key, const QString &fallback = {}) const;

    QSettings *m_settings = nullptr;
    TranslationProvider *m_provider = nullptr;
    QString m_providerId = QStringLiteral("google");
};
