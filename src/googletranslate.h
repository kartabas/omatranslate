#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVector>

// One piece of a long passage, and the whitespace that separated it from the
// piece before it, so the translated pieces can be put back together the way
// the original was laid out.
struct TranslationChunk {
    QString text;
    QString join;
};

// Google Translate through the endpoint its own browser extension uses. No key
// and no account: the person asked for this and the app says plainly that the
// text they select leaves the machine.
class GoogleTranslate : public QObject {
    Q_OBJECT

public:
    explicit GoogleTranslate(const QUrl &endpoint = {}, QObject *parent = nullptr);
    ~GoogleTranslate() override = default;

    virtual void translate(const QString &text, const QString &from, const QString &to);
    void cancel();

    static QUrl defaultEndpoint();
    static QVector<TranslationChunk> chunk(const QString &text, int limit = 3000);

    // Returns the translation, sets the detected source language, and leaves an
    // error message when Google answered with something other than a result.
    static QString parse(const QByteArray &body, QString *detected, QString *error);

signals:
    void finished(const QString &translation, const QString &detected);
    void failed(const QString &message);

private:
    void sendNext();
    void handleReply();
    void reset();

    QUrl m_endpoint;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QVector<TranslationChunk> m_chunks;
    int m_index = 0;
    QString m_result;
    QString m_detected;
    QString m_from;
    QString m_to;
};
