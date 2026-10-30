#include "googletranslate.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

namespace {

// Google answers in a few shapes depending on the text and the mood of the
// endpoint; this walks whichever one arrives and collects the pieces.
void collect(const QJsonValue &value, QStringList &parts, QString &language) {
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        if (!array.isEmpty() && array.at(0).isString()) {
            parts << array.at(0).toString();
            if (language.isEmpty() && array.size() > 1 && array.at(1).isString())
                language = array.at(1).toString();
            return;
        }
        for (const QJsonValue &entry : array)
            collect(entry, parts, language);
    } else if (value.isString()) {
        parts << value.toString();
    }
}

}

GoogleTranslate::GoogleTranslate(const QUrl &endpoint, QObject *parent)
    : QObject(parent), m_endpoint(endpoint.isValid() ? endpoint : defaultEndpoint()) {}

QUrl GoogleTranslate::defaultEndpoint() {
    // clients5 is the host the Translate browser extension uses. The shared
    // translate_a endpoints answer this machine with an anti-bot page.
    return QUrl(QStringLiteral("https://clients5.google.com/translate_a/t"));
}

void GoogleTranslate::translate(const QString &text, const QString &from, const QString &to) {
    cancel();
    m_chunks = chunk(text);
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

void GoogleTranslate::cancel() {
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

void GoogleTranslate::sendNext() {
    if (m_index >= m_chunks.size()) {
        emit finished(m_result.trimmed(), m_detected);
        reset();
        return;
    }

    const TranslationChunk chunk = m_chunks.at(m_index);
    QUrl url(m_endpoint);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client"), QStringLiteral("dict-chrome-ex"));
    query.addQueryItem(QStringLiteral("sl"), m_from.isEmpty() ? QStringLiteral("auto") : m_from);
    query.addQueryItem(QStringLiteral("tl"), m_to);
    query.addQueryItem(QStringLiteral("q"), chunk.text);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                                     "(KHTML, like Gecko) Chrome/126.0 Safari/537.36"));
    request.setTransferTimeout(15000);

    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, &GoogleTranslate::handleReply);
}

void GoogleTranslate::handleReply() {
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() == QNetworkReply::OperationCanceledError)
        return;
    if (reply->error() != QNetworkReply::NoError) {
        const QString message = tr("Couldn't reach Google Translate: %1").arg(reply->errorString());
        cancel();
        emit failed(message);
        return;
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 429) {
        cancel();
        emit failed(tr("Google is limiting requests. Wait a moment and try again."));
        return;
    }
    if (status != 200) {
        cancel();
        emit failed(tr("Google answered with HTTP %1.").arg(status));
        return;
    }

    QString detected, error;
    const QString translated = parse(reply->readAll(), &detected, &error);
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

void GoogleTranslate::reset() {
    m_chunks.clear();
    m_index = 0;
    m_result.clear();
    m_from.clear();
    m_to.clear();
}

QVector<TranslationChunk> GoogleTranslate::chunk(const QString &text, int limit) {
    QVector<TranslationChunk> chunks;
    if (limit < 200)
        limit = 200;

    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    static const QRegularExpression paragraphBreak(QStringLiteral("\\n\\s*\\n"));
    const QString sentencePattern = QStringLiteral("(?<=[.!?")
        + QChar(0x2026) + QChar(0x3002) + QChar(0xFF01) + QChar(0xFF1F)
        + QStringLiteral("])\\s+");
    static const QRegularExpression sentenceBreak(sentencePattern);
    static const QRegularExpression whitespace(QStringLiteral("\\s"));

    // Long sentences are cut at whitespace, never inside a word, and the cut
    // is preferred at a sentence end so each request holds whole thoughts.
    const auto piecesFor = [limit](const QString &paragraph) {
        QStringList pieces;
        if (paragraph.size() <= limit) {
            pieces << paragraph;
            return pieces;
        }
        QString current;
        const auto flush = [&] {
            if (!current.trimmed().isEmpty())
                pieces << current.trimmed();
            current.clear();
        };
        for (const QString &sentence : paragraph.split(sentenceBreak)) {
            if (sentence.size() > limit) {
                flush();
                QString rest = sentence;
                while (rest.size() > limit) {
                    int cut = rest.lastIndexOf(whitespace, limit);
                    if (cut <= 0)
                        cut = limit;
                    pieces << rest.left(cut).trimmed();
                    rest = rest.mid(cut).trimmed();
                }
                if (!rest.isEmpty())
                    pieces << rest;
                continue;
            }
            if (!current.isEmpty() && current.size() + 1 + sentence.size() > limit)
                flush();
            if (!current.isEmpty())
                current += QLatin1Char(' ');
            current += sentence;
        }
        flush();
        return pieces;
    };

    bool first = true;
    for (const QString &raw : normalized.split(paragraphBreak)) {
        const QString paragraph = raw.trimmed();
        if (paragraph.isEmpty())
            continue;
        const QStringList pieces = piecesFor(paragraph);
        for (int i = 0; i < pieces.size(); ++i) {
            TranslationChunk chunk;
            chunk.text = pieces.at(i);
            chunk.join = first ? QString() : i == 0 ? QStringLiteral("\n\n") : QStringLiteral(" ");
            chunks << chunk;
            first = false;
        }
    }
    return chunks;
}

QString GoogleTranslate::parse(const QByteArray &body, QString *detected, QString *error) {
    if (detected)
        detected->clear();
    if (error)
        error->clear();

    const QByteArray trimmed = body.trimmed();
    if (trimmed.isEmpty()) {
        if (error)
            *error = tr("Google sent an empty answer.");
        return {};
    }
    // When Google blocks the request it answers with an HTML "Sorry" page.
    if (!trimmed.startsWith('[') && !trimmed.startsWith('{')) {
        if (error)
            *error = tr("Google refused the translation. Wait a moment and try again.");
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(trimmed, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = tr("Google sent a translation this app couldn't read.");
        return {};
    }

    QString language;
    QString translated;
    if (document.isObject()) {
        // The older web endpoint answers {"sentences": [{"trans": ...}], "src": ...}.
        // Each of its sentences is whole, so they are joined with a space.
        const QJsonObject object = document.object();
        if (object.contains(QStringLiteral("sentences"))) {
            QStringList sentences;
            for (const QJsonValue &value : object.value(QStringLiteral("sentences")).toArray())
                sentences << value.toObject().value(QStringLiteral("trans")).toString();
            translated = sentences.join(QLatin1Char(' '));
            language = object.value(QStringLiteral("src")).toString();
        } else if (object.contains(QStringLiteral("translation"))) {
            translated = object.value(QStringLiteral("translation")).toString();
        }
    } else {
        // The array shape splits a sentence into pieces that belong together.
        QStringList pieces;
        collect(document.array(), pieces, language);
        translated = pieces.join(QString());
    }

    const QString translation = translated.trimmed();
    if (translation.isEmpty()) {
        if (error)
            *error = tr("Google sent an empty translation.");
        return {};
    }
    if (detected)
        *detected = language;
    return translation;
}
