#include "translationservice.h"

#include <QDir>
#include <QFile>
#include <QUrl>

#include "deepltranslate.h"
#include "googletranslate.h"
#include "libretranslate.h"

namespace {

const QString GoogleId = QStringLiteral("google");
const QString DeepLId = QStringLiteral("deepl");
const QString LibreId = QStringLiteral("libretranslate");

bool knownProvider(const QString &id) {
    return id == GoogleId || id == DeepLId || id == LibreId;
}

}

TranslationService::TranslationService(QSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
    m_providerId = readString(QStringLiteral("provider"), GoogleId);
    if (!knownProvider(m_providerId))
        m_providerId = GoogleId;
    rebuild();
}

TranslationService::~TranslationService() {
    if (m_provider) {
        m_provider->disconnect(this);
        delete m_provider;
        m_provider = nullptr;
    }
}

QString TranslationService::readString(const QString &key, const QString &fallback) const {
    if (!m_settings)
        return fallback;
    return m_settings->value(key, fallback).toString();
}

QString TranslationService::deepLKey() const {
    return readString(QStringLiteral("deeplApiKey"));
}

QString TranslationService::libreUrl() const {
    if (!m_settings)
        return LibreTranslate::defaultEndpoint().toString();
    return m_settings->value(QStringLiteral("libreUrl"),
                             LibreTranslate::defaultEndpoint().toString())
        .toString();
}

QString TranslationService::libreKey() const {
    return readString(QStringLiteral("libreApiKey"));
}

QString TranslationService::providerName() const {
    if (m_providerId == DeepLId)
        return tr("DeepL");
    if (m_providerId == LibreId)
        return tr("LibreTranslate");
    return tr("Google Translate");
}

QString TranslationService::endpoint() const {
    return endpointFor(m_providerId);
}

QString TranslationService::endpointFor(const QString &id) const {
    if (id == DeepLId)
        return DeepLTranslate::endpointFor(deepLKey()).toString();
    if (id == LibreId)
        return LibreTranslate::endpointFrom(libreUrl()).toString();
    return GoogleTranslate::defaultEndpoint().toString();
}

QString TranslationService::settingsPath() const {
    if (!m_settings)
        return {};
    QString path = m_settings->fileName();
    const QString home = QDir::homePath();
    if (!home.isEmpty() && path.startsWith(home + QLatin1Char('/')))
        path = QStringLiteral("~") + path.mid(home.size());
    return path;
}

QVariantList TranslationService::providers() const {
    return {
        QVariantMap{
            {QStringLiteral("id"), GoogleId},
            {QStringLiteral("name"), tr("Google Translate")},
            {QStringLiteral("detail"), tr("No account or key")},
        },
        QVariantMap{
            {QStringLiteral("id"), DeepLId},
            {QStringLiteral("name"), tr("DeepL")},
            {QStringLiteral("detail"), tr("Your own API key")},
        },
        QVariantMap{
            {QStringLiteral("id"), LibreId},
            {QStringLiteral("name"), tr("LibreTranslate")},
            {QStringLiteral("detail"), tr("Your own server")},
        },
    };
}

QString TranslationService::disclosure() const {
    const QString retention = tr("Nothing is translated until you press Translate. Omatranslate "
                                 "keeps no copy after you quit unless you export it.");
    if (m_providerId == DeepLId) {
        const QString where = tr("Selected text is sent to DeepL at %1 with your API key. The key "
                                 "is stored in %2, readable only by you.")
                                  .arg(endpoint(), settingsPath());
        return where + QLatin1Char('\n') + retention;
    }
    if (m_providerId == LibreId) {
        const QString where = tr("Selected text is sent to the LibreTranslate server at %1. The "
                                 "address and optional API key are stored in %2, readable only by "
                                 "you.")
                                  .arg(endpoint(), settingsPath());
        return where + QLatin1Char('\n') + retention;
    }
    const QString where = tr("Selected text is sent to Google Translate at %1. No account or API "
                             "key is used; Google may keep the request under its own policy.")
                              .arg(endpoint());
    return where + QLatin1Char('\n') + retention;
}

void TranslationService::translate(const QString &text, const QString &from, const QString &to) {
    if (!m_provider)
        rebuild();
    if (m_provider)
        m_provider->translate(text, from, to);
}

void TranslationService::cancel() {
    if (m_provider)
        m_provider->cancel();
}

void TranslationService::setProvider(const QString &id) {
    if (!knownProvider(id) || id == m_providerId)
        return;
    m_providerId = id;
    saveSetting(QStringLiteral("provider"), id);
    rebuild();
}

void TranslationService::setDeepLKey(const QString &key) {
    const QString trimmed = key.trimmed();
    if (trimmed == deepLKey())
        return;
    saveSetting(QStringLiteral("deeplApiKey"), trimmed);
    if (m_providerId == DeepLId)
        rebuild();
    else
        emit changed();
}

void TranslationService::setLibreUrl(const QString &url) {
    const QString trimmed = url.trimmed();
    if (trimmed == libreUrl())
        return;
    saveSetting(QStringLiteral("libreUrl"), trimmed);
    if (m_providerId == LibreId)
        rebuild();
    else
        emit changed();
}

void TranslationService::setLibreKey(const QString &key) {
    const QString trimmed = key.trimmed();
    if (trimmed == libreKey())
        return;
    saveSetting(QStringLiteral("libreApiKey"), trimmed);
    if (m_providerId == LibreId)
        rebuild();
    else
        emit changed();
}

void TranslationService::rebuild() {
    if (m_provider) {
        m_provider->disconnect(this);
        m_provider->deleteLater();
        m_provider = nullptr;
    }

    if (m_providerId == DeepLId)
        m_provider = new DeepLTranslate(deepLKey(), this);
    else if (m_providerId == LibreId)
        m_provider = new LibreTranslate(LibreTranslate::endpointFrom(libreUrl()), libreKey(), this);
    else
        m_provider = new GoogleTranslate({}, this);

    connect(m_provider, &TranslationProvider::finished, this, &TranslationService::finished);
    connect(m_provider, &TranslationProvider::failed, this, &TranslationService::failed);
    emit changed();
}

void TranslationService::saveSetting(const QString &key, const QVariant &value) {
    if (!m_settings) {
        emit changed();
        return;
    }
    m_settings->setValue(key, value);
    m_settings->sync();
    // The file holds an API key, so it belongs to this user alone.
    const QString path = m_settings->fileName();
    if (QFile::exists(path))
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
    emit changed();
}
