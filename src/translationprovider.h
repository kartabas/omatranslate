#pragma once

#include <QObject>
#include <QString>
#include <QVector>

// One piece of a long passage, and the whitespace that separated it from the
// piece before it, so the translated pieces can be put back together the way
// the original was laid out.
struct TranslationChunk {
    QString text;
    QString join;
};

// Split a passage into requests a translation service will accept, preferring
// paragraph and sentence boundaries over cuts inside a word.
QVector<TranslationChunk> chunkText(const QString &text, int limit = 3000);

// A translation service the person can pick in Settings. Every provider takes
// the same call and answers with the same two signals, so the rest of the app
// does not care which one is selected.
class TranslationProvider : public QObject {
    Q_OBJECT

public:
    explicit TranslationProvider(QObject *parent = nullptr);
    ~TranslationProvider() override = default;

    virtual void translate(const QString &text, const QString &from, const QString &to) = 0;
    virtual void cancel() = 0;

    // For Settings: what to call this service, and the exact address the
    // selected text is sent to.
    virtual QString name() const = 0;
    virtual QString endpoint() const = 0;

signals:
    void finished(const QString &translation, const QString &detected);
    void failed(const QString &message);
};
