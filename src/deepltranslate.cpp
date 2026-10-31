#include "deepltranslate.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace {

// DeepL writes its language codes in upper case and does not use Google's
// regional Chinese split. Everything else is passed through upper-cased, which
// is what the API documents.
const QHash<QString, QString> &deepLTargets() {
    static const QHash<QString, QString> targets{
        {"pt", "PT-PT"}, {"pt-BR", "PT-BR"}, {"zh-CN", "ZH"}, {"zh-TW", "ZH"},
        {"en", "EN-US"}, {"nb", "NB"}, {"no", "NB"},
    };
    return targets;
}

QString normalizeDetected(const QString &code) {
    const QString lower = code.trimmed().toLower();
    if (lower == QLatin1String("zh"))
        return QStringLiteral("zh-CN");
    return lower;
}

}

DeepLTranslate::DeepLTranslate(const QString &apiKey, QObject *parent)
    : TranslationProvider(parent), m_key(apiKey.trimmed()), m_endpoint(endpointFor(m_key)) {}

QUrl DeepLTranslate::endpointFor(const QString &apiKey) {
    return QUrl(apiKey.trimmed().endsWith(QLatin1String(":fx"))
                    ? QStringLiteral("https://api-free.deepl.com/v2/translate")
                    : QStringLiteral("https://api.deepl.com/v2/translate"));
}

QString DeepLTranslate::name() const {
    return tr("DeepL");
}

QString DeepLTranslate::endpoint() const {
    return m_endpoint.toString();
}

QString DeepLTranslate::sourceCode(const QString &code) {
    if (code.isEmpty() || code == QLatin1String("auto"))
        return {};
    return targetCode(code);
}

QString DeepLTranslate::targetCode(const QString &code) {
    if (code.isEmpty() || code == QLatin1String("auto"))
        return {};
    const auto &targets = deepLTargets();
    if (targets.contains(code))
        return targets.value(code);
    return code.toUpper();
}

void DeepLTranslate::translate(const QString &text, const QString &from, const QString &to) {
    cancel();
    if (m_key.isEmpty()) {
        emit failed(tr("Add a DeepL API key in Settings first."));
        return;
    }
    m_chunks = chunkText(text, 4000);
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

void DeepLTranslate::cancel() {
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

void DeepLTranslate::sendNext() {
    if (m_index >= m_chunks.size()) {
        emit finished(m_result.trimmed(), m_detected);
        reset();
        return;
    }

    const TranslationChunk chunk = m_chunks.at(m_index);
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("auth_key"), m_key);
    form.addQueryItem(QStringLiteral("text"), chunk.text);
    const QString source = sourceCode(m_from);
    if (!source.isEmpty())
        form.addQueryItem(QStringLiteral("source_lang"), source);
    form.addQueryItem(QStringLiteral("target_lang"), targetCode(m_to));

    QNetworkRequest request(m_endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setTransferTimeout(20000);

    m_reply = m_network.post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(m_reply, &QNetworkReply::finished, this, &DeepLTranslate::handleReply);
}

void DeepLTranslate::handleReply() {
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError)
        return;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (status == 403 || status == 456) {
        cancel();
        emit failed(tr("DeepL refused the request. Check the API key and its quota."));
        return;
    }
    if (status == 429) {
        cancel();
        emit failed(tr("DeepL is limiting requests. Wait a moment and try again."));
        return;
    }
    if (status != 200) {
        QString detected, error;
        parse(body, &detected, &error);
        cancel();
        emit failed(error.isEmpty() ? tr("DeepL answered with HTTP %1.").arg(status) : error);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        cancel();
        emit failed(tr("Couldn't reach DeepL: %1").arg(reply->errorString()));
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

void DeepLTranslate::reset() {
    m_chunks.clear();
    m_index = 0;
    m_result.clear();
    m_from.clear();
    m_to.clear();
}

QString DeepLTranslate::parse(const QByteArray &body, QString *detected, QString *error) {
    if (detected)
        detected->clear();
    if (error)
        error->clear();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = tr("DeepL sent an answer this app couldn't read.");
        return {};
    }

    const QJsonObject object = document.object();
    if (object.contains(QStringLiteral("message"))) {
        if (error)
            *error = tr("DeepL answered: %1").arg(object.value(QStringLiteral("message")).toString());
        return {};
    }

    const QJsonArray translations = object.value(QStringLiteral("translations")).toArray();
    if (translations.isEmpty()) {
        if (error)
            *error = tr("DeepL sent an empty translation.");
        return {};
    }

    const QJsonObject first = translations.first().toObject();
    const QString translation = first.value(QStringLiteral("text")).toString().trimmed();
    if (translation.isEmpty()) {
        if (error)
            *error = tr("DeepL sent an empty translation.");
        return {};
    }
    if (detected)
        *detected = normalizeDetected(first.value(QStringLiteral("detected_source_language")).toString());
    return translation;
}
