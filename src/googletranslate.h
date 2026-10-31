#pragma once

#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

#include "translationprovider.h"

// Google Translate through the endpoint its own browser extension uses. No key
// and no account: the person asked for this and the app says plainly that the
// text they select leaves the machine.
class GoogleTranslate : public TranslationProvider {
    Q_OBJECT

public:
    explicit GoogleTranslate(const QUrl &endpoint = {}, QObject *parent = nullptr);
    ~GoogleTranslate() override = default;

    void translate(const QString &text, const QString &from, const QString &to) override;
    void cancel() override;
    QString name() const override;
    QString endpoint() const override;

    static QUrl defaultEndpoint();
    static QVector<TranslationChunk> chunk(const QString &text, int limit = 3000);

    // Returns the translation, sets the detected source language, and leaves an
    // error message when Google answered with something other than a result.
    static QString parse(const QByteArray &body, QString *detected, QString *error);

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
