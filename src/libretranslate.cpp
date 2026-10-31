#include "libretranslate.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

LibreTranslate::LibreTranslate(const QUrl &endpoint, const QString &apiKey, QObject *parent)
    : TranslationProvider(parent),
      m_endpoint(endpoint.isValid() ? endpoint : defaultEndpoint()),
      m_key(apiKey.trimmed()) {}

QUrl LibreTranslate::defaultEndpoint() {
    return QUrl(QStringLiteral("https://libretranslate.com/translate"));
}

QUrl LibreTranslate::endpointFrom(const QString &value) {
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return defaultEndpoint();
    QUrl url = QUrl::fromUserInput(trimmed);
    // A person may type just the host; /translate is the path every
    // LibreTranslate server exposes.
    if (url.path().isEmpty() || url.path() == QLatin1String("/"))
        url.setPath(QStringLiteral("/translate"));
    return url;
}

QString LibreTranslate::name() const {
    return tr("LibreTranslate");
}

QString LibreTranslate::endpoint() const {
    return m_endpoint.toString();
}

void LibreTranslate::translate(const QString &text, const QString &from, const QString &to) {
    cancel();
    m_chunks = chunkText(text, 3000);
    m_index = 0;
    m_result.clear();
    m_detected.clear();
    m_from = from;
    m_to = to;
    if (m_chunks.isEmpty()) {
        emit failed(tr("There's nothing to translate."));
        return;
    }
    sendNext();
}

void LibreTranslate::cancel() {
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_chunks.clear();
    m_index = 0;
    m_result.clear();
}

void LibreTranslate::sendNext() {
    if (m_index >= m_chunks.size()) {
        emit finished(m_result.trimmed(), m_detected);
        reset();
        return;
    }

    const TranslationChunk chunk = m_chunks.at(m_index);
    QJsonObject payload{
        {QStringLiteral("q"), chunk.text},
        {QStringLiteral("source"), m_from.isEmpty() ? QStringLiteral("auto") : m_from},
        {QStringLiteral("target"), m_to},
        {QStringLiteral("format"), QStringLiteral("text")},
    };
    if (!m_key.isEmpty())
        payload.insert(QStringLiteral("api_key"), m_key);

    QNetworkRequest request(m_endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setTransferTimeout(20000);

    m_reply = m_network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, &LibreTranslate::handleReply);
}

void LibreTranslate::handleReply() {
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError)
        return;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (status == 429) {
        cancel();
        emit failed(tr("LibreTranslate is limiting requests. Wait a moment and try again."));
        return;
    }
    if (status != 200) {
        QString detected, error;
        parse(body, &detected, &error);
        cancel();
        emit failed(error.isEmpty() ? tr("LibreTranslate answered with HTTP %1.").arg(status) : error);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        cancel();
        emit failed(tr("Couldn't reach LibreTranslate: %1").arg(reply->errorString()));
        return;
    }

    QString detected, error;
    const QString translated = parse(body, &detected, &error);
    if (!error.isEmpty()) {
        cancel();
        emit failed(error);
        return;
    }
    if (m_detected.isEmpty())
        m_detected = detected;
    m_result += m_chunks.at(m_index).join + translated;
    ++m_index;
    sendNext();
}

void LibreTranslate::reset() {
    m_chunks.clear();
    m_index = 0;
    m_result.clear();
    m_from.clear();
    m_to.clear();
}

QString LibreTranslate::parse(const QByteArray &body, QString *detected, QString *error) {
    if (detected)
        detected->clear();
    if (error)
        error->clear();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = tr("LibreTranslate sent an answer this app couldn't read.");
        return {};
    }

    const QJsonObject object = document.object();
    const QString apiError = object.value(QStringLiteral("error")).toString();
    if (!apiError.isEmpty()) {
        if (error)
            *error = tr("LibreTranslate answered: %1").arg(apiError);
        return {};
    }

    const QString translation = object.value(QStringLiteral("translatedText")).toString().trimmed();
    if (translation.isEmpty()) {
        if (error)
            *error = tr("LibreTranslate sent an empty translation.");
        return {};
    }

    const QJsonObject language = object.value(QStringLiteral("detectedLanguage")).toObject();
    if (detected)
        *detected = language.value(QStringLiteral("language")).toString();
    return translation;
}
