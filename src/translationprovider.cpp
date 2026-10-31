#include "translationprovider.h"

#include <QRegularExpression>

TranslationProvider::TranslationProvider(QObject *parent) : QObject(parent) {}

QVector<TranslationChunk> chunkText(const QString &text, int limit) {
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
