#pragma once

#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

#include "translationprovider.h"

// DeepL's documented API. A key ending in :fx is the free tier, which lives on
// another host; every other key uses the paid host. The key is stored in the
// app's own settings file, never in the repository.
class DeepLTranslate : public TranslationProvider {
    Q_OBJECT

public:
    explicit DeepLTranslate(const QString &apiKey, QObject *parent = nullptr);
    ~DeepLTranslate() override = default;

    void translate(const QString &text, const QString &from, const QString &to) override;
    void cancel() override;
    QString name() const override;
    QString endpoint() const override;

    static QUrl endpointFor(const QString &apiKey);
    static QString sourceCode(const QString &code);
    static QString targetCode(const QString &code);

    // Returns the translation and the detected source language, or an error.
    static QString parse(const QByteArray &body, QString *detected, QString *error);

private:
    void sendNext();
    void handleReply();
    void reset();

    QString m_key;
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
