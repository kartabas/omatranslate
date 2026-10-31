#pragma once

#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

#include "translationprovider.h"

// A LibreTranslate server, either the public one or one the person runs. The
// address and the optional API key both come from Settings.
class LibreTranslate : public TranslationProvider {
    Q_OBJECT

public:
    LibreTranslate(const QUrl &endpoint, const QString &apiKey, QObject *parent = nullptr);
    ~LibreTranslate() override = default;

    void translate(const QString &text, const QString &from, const QString &to) override;
    void cancel() override;
    QString name() const override;
    QString endpoint() const override;

    static QUrl defaultEndpoint();
    static QUrl endpointFrom(const QString &value);
    static QString parse(const QByteArray &body, QString *detected, QString *error);

private:
    void sendNext();
    void handleReply();
    void reset();

    QUrl m_endpoint;
    QString m_key;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QVector<TranslationChunk> m_chunks;
    int m_index = 0;
    QString m_result;
    QString m_detected;
    QString m_from;
    QString m_to;
};
