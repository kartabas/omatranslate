#include "backend.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

#include "filepicker.h"
#include "googletranslate.h"
#include "languages.h"
#include "selectionwatcher.h"

namespace {

// A highlighted passage is a few paragraphs, not a book. Anything longer is
// cut at a word so a stray select-all can't flood the translator.
constexpr int MaximumSelection = 10000;
constexpr int MaximumHistory = 300;
constexpr int MaximumCache = 120;

QString shortened(const QString &text) {
    if (text.size() <= MaximumSelection)
        return text;
    int cut = text.lastIndexOf(QRegularExpression(QStringLiteral("\\s")), MaximumSelection);
    if (cut < MaximumSelection / 2)
        cut = MaximumSelection;
    return text.left(cut).trimmed();
}

}

Backend::Backend(FilePicker *picker, GoogleTranslate *translate, SelectionWatcher *watcher,
                 QSettings *settings, const QString &defaultTarget, QObject *parent)
    : QObject(parent), m_picker(picker), m_translate(translate), m_settings(settings) {
    const QString fallbackTarget = isSupportedLanguage(defaultTarget) ? defaultTarget : QStringLiteral("en");
    if (m_settings) {
        m_source = m_settings->value("sourceLanguage", "auto").toString();
        m_target = m_settings->value("targetLanguage", fallbackTarget).toString();
        m_autoTranslate = m_settings->value("autoTranslate", true).toBool();
    } else {
        m_target = fallbackTarget;
    }
    if (m_source != QLatin1String("auto") && !isSupportedLanguage(m_source))
        m_source = QStringLiteral("auto");
    if (!isSupportedLanguage(m_target))
        m_target = fallbackTarget;

    if (m_picker) {
        connect(m_picker, &FilePicker::pdfSelected, this, [this](const QUrl &url) { openUrl(url); });
        connect(m_picker, &FilePicker::saveSelected, this, [this](const QUrl &url) { saveTo(url.toLocalFile()); });
        connect(m_picker, &FilePicker::failed, this, &Backend::setNotice);
    }
    if (m_translate) {
        connect(m_translate, &GoogleTranslate::finished, this, [this](const QString &translation,
                                                                     const QString &detected) {
            // A reply that arrives after the passage was replaced is stale, and
            // replacing the new passage with it would be worse than waiting.
            if (m_pendingText.isEmpty())
                return;
            const QString text = m_pendingText;
            const QString target = m_pendingTarget;
            const QString document = m_pendingDocument;
            remember(translation, detected);
            m_pendingText.clear();
            setStatus(QStringLiteral("done"));
            emit changed();
            addHistory(text, translation, detected, target, document);
        });
        connect(m_translate, &GoogleTranslate::failed, this, [this](const QString &message) {
            m_pendingText.clear();
            m_error = message;
            setStatus(QStringLiteral("error"));
            emit changed();
        });
    }
    if (watcher) {
        m_watchError = watcher->error();
        connect(watcher, &SelectionWatcher::textChanged, this, &Backend::handleSelection);
    }

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(&m_debounce, &QTimer::timeout, this, &Backend::startTranslation);

    m_noticeTimer.setSingleShot(true);
    m_noticeTimer.setInterval(5000);
    connect(&m_noticeTimer, &QTimer::timeout, this, [this] { setNotice({}); });
}

QString Backend::clean(const QString &selection) {
    QString text = selection;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    text.remove(QChar(0x00AD));  // a soft hyphen is a typesetting hint, not a word break

    // Paragraphs stay paragraphs; inside one, the reader's line breaks are
    // joined back into sentences, and words hyphenated across lines are
    // put together so the translator sees the word it knows.
    QStringList paragraphs;
    const auto parts = text.split(QRegularExpression(QStringLiteral("\\n\\s*\\n")));
    for (QString paragraph : parts) {
        paragraph.replace(QRegularExpression(QStringLiteral("(\\p{L})-[ \\t]*\\n\\s*(\\p{L})")),
                          QStringLiteral("\\1\\2"));
        paragraph.replace(QRegularExpression(QStringLiteral("[ \\t]*\\n[ \\t]*")), QStringLiteral(" "));
        paragraph.replace(QRegularExpression(QStringLiteral("[ \\t]+")), QStringLiteral(" "));
        paragraph = paragraph.trimmed();
        if (!paragraph.isEmpty())
            paragraphs << paragraph;
    }
    return paragraphs.join(QStringLiteral("\n\n"));
}

QVariantList Backend::languages() const {
    return languageOptions(true);
}

QString Backend::languageName(const QString &code) const {
    return ::languageName(code);
}

void Backend::setDebounceInterval(int milliseconds) {
    m_debounce.setInterval(qMax(0, milliseconds));
}

void Backend::openDocument() {
    if (!m_picker)
        return;
    const QString folder = m_settings ? m_settings->value("folder").toString() : QString();
    m_picker->openPdf(folder);
}

bool Backend::openUrl(const QUrl &url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QFileInfo info(path);
    if (!info.isFile()) {
        setNotice(tr("Can't find %1.").arg(info.fileName().isEmpty() ? path : info.fileName()));
        return false;
    }
    if (info.suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0) {
        setNotice(tr("%1 isn't a PDF.").arg(info.fileName()));
        return false;
    }

    m_documentPath = info.absoluteFilePath();
    m_documentName = info.fileName();
    if (m_settings)
        m_settings->setValue("folder", info.absolutePath());
    emit documentChanged();
    openInReader();
    return true;
}

void Backend::openInReader() {
    if (!hasDocument())
        return;
    const QString opener = QStandardPaths::findExecutable("xdg-open");
    if (opener.isEmpty()) {
        setNotice(tr("Couldn't open the PDF, because xdg-open isn't installed."));
        return;
    }
    if (!QProcess::startDetached(opener, {m_documentPath}))
        setNotice(tr("Couldn't open %1 in your PDF reader.").arg(m_documentName));
}

void Backend::handleSelection(const QString &text) {
    const QString cleaned = clean(text);
    if (cleaned == m_selection)
        return;

    m_debounce.stop();
    m_selection = cleaned;
    m_translation.clear();
    m_detected.clear();
    m_error.clear();
    m_pendingText.clear();
    setStatus(cleaned.isEmpty() ? QStringLiteral("idle")
                                : m_autoTranslate ? QStringLiteral("waiting") : QStringLiteral("idle"));
    emit changed();
    if (!cleaned.isEmpty() && m_autoTranslate)
        m_debounce.start();
}

void Backend::translateNow() {
    if (m_selection.isEmpty()) {
        setNotice(tr("Highlight a passage in your PDF reader first."));
        return;
    }
    startTranslation();
}

void Backend::retry() {
    m_cache.clear();
    m_cacheDetected.clear();
    translateNow();
}

void Backend::startTranslation() {
    m_debounce.stop();
    if (m_selection.isEmpty() || !m_translate)
        return;

    const QString text = shortened(m_selection);
    if (text.size() < m_selection.size())
        setNotice(tr("That passage is long, so only its first %1 characters were translated.").arg(MaximumSelection));

    m_error.clear();
    setStatus(QStringLiteral("translating"));
    emit changed();

    m_pendingText = text;
    m_pendingTarget = m_target;
    m_pendingDocument = m_documentName;

    const QString key = m_source + QLatin1Char('\n') + m_target + QLatin1Char('\n') + text;
    if (m_cache.contains(key)) {
        const QString translation = m_cache.value(key);
        const QString detected = m_cacheDetected.value(key);
        const QString target = m_pendingTarget;
        const QString document = m_pendingDocument;
        m_pendingText.clear();
        m_translation = translation;
        m_detected = detected;
        setStatus(QStringLiteral("done"));
        emit changed();
        addHistory(text, translation, detected, target, document);
        return;
    }

    m_translate->translate(text, m_source, m_target);
}

void Backend::remember(const QString &translation, const QString &detected) {
    m_translation = translation;
    m_detected = detected;
    if (m_cache.size() >= MaximumCache) {
        m_cache.clear();
        m_cacheDetected.clear();
    }
    const QString key = m_source + QLatin1Char('\n') + m_pendingTarget + QLatin1Char('\n') + m_pendingText;
    m_cache.insert(key, translation);
    m_cacheDetected.insert(key, detected);
}

void Backend::copyTranslation() {
    if (m_translation.isEmpty()) {
        setNotice(tr("There's no translation to copy yet."));
        return;
    }
    QGuiApplication::clipboard()->setText(m_translation);
    setNotice(tr("Translation copied."));
}

void Backend::exportTranslations() {
    if (m_history.isEmpty()) {
        setNotice(tr("Nothing has been translated yet."));
        return;
    }
    if (!m_picker)
        return;

    const QFileInfo document(m_documentPath);
    const QString name = document.isFile() ? document.completeBaseName() + QStringLiteral(".omatranslate.md")
                                           : QStringLiteral("omatranslate.md");
    QString folder = m_settings ? m_settings->value("exportFolder").toString() : QString();
    if (folder.isEmpty() || !QDir(folder).exists())
        folder = document.isFile() ? document.absolutePath()
                                   : QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (folder.isEmpty() || !QDir(folder).exists())
        folder = QDir::homePath();
    m_picker->saveMarkdown(QUrl::fromLocalFile(QDir(folder).filePath(name)));
}

void Backend::saveTo(const QString &path) {
    if (path.isEmpty() || m_history.isEmpty())
        return;
    QString error;
    const QString markdown = exportMarkdown(m_history, m_documentName);
    if (!writeAtomically(path, markdown.toUtf8(), &error)) {
        setNotice(tr("Couldn't save the translations: %1").arg(error));
        return;
    }
    const QFileInfo saved(path);
    if (m_settings) {
        m_settings->setValue("exportFolder", saved.absolutePath());
        m_settings->sync();
    }
    setUnsaved(false);
    setNotice(tr("Saved %1.").arg(saved.fileName()));
}

bool Backend::writeAtomically(const QString &path, const QByteArray &data, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString Backend::exportMarkdown(const QVariantList &history, const QString &documentName) {
    if (history.isEmpty())
        return {};

    QString markdown = QStringLiteral("# Translations\n\n");
    if (!documentName.isEmpty())
        markdown += QCoreApplication::translate("Backend", "- **Document:** %1\n").arg(documentName);
    markdown += QCoreApplication::translate("Backend", "- **Exported:** %1\n\n")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));

    // The list is newest first; a file reads best oldest first.
    QVariantList ordered = history;
    std::reverse(ordered.begin(), ordered.end());
    int number = 1;
    for (const QVariant &value : ordered) {
        const QVariantMap entry = value.toMap();
        const QString from = languageEnglishName(entry.value("from").toString());
        const QString to = languageEnglishName(entry.value("to").toString());
        const QDateTime time = QDateTime::fromString(entry.value("time").toString(), Qt::ISODate);
        markdown += QStringLiteral("## %1 · %2 → %3")
                        .arg(number++)
                        .arg(from.isEmpty() ? QStringLiteral("—") : from, to);
        if (time.isValid())
            markdown += QStringLiteral(" · ") + time.toString(QStringLiteral("HH:mm"));
        markdown += QStringLiteral("\n\n");

        const QString text = entry.value("text").toString();
        for (const QString &line : text.split(QLatin1Char('\n')))
            markdown += QStringLiteral("> ") + line + QLatin1Char('\n');
        markdown += QLatin1Char('\n');
        markdown += entry.value("translation").toString().trimmed();
        markdown += QStringLiteral("\n\n");
    }
    return markdown;
}

void Backend::addHistory(const QString &text, const QString &translation, const QString &detected,
                         const QString &target, const QString &document) {
    // The same passage translated again is the same note, moved to the top
    // rather than copied, so the export never repeats itself.
    for (int i = 0; i < m_history.size(); ++i) {
        const QVariantMap existing = m_history.at(i).toMap();
        if (existing.value("text").toString() == text && existing.value("to").toString() == target) {
            m_history.removeAt(i);
            break;
        }
    }

    QVariantMap entry;
    entry.insert(QStringLiteral("text"), text);
    entry.insert(QStringLiteral("translation"), translation);
    entry.insert(QStringLiteral("from"), detected);
    entry.insert(QStringLiteral("to"), target);
    entry.insert(QStringLiteral("document"), document);
    entry.insert(QStringLiteral("time"), QDateTime::currentDateTime().toString(Qt::ISODate));
    m_history.prepend(entry);
    while (m_history.size() > MaximumHistory)
        m_history.removeLast();

    setUnsaved(true);
    emit historyChanged();
}

void Backend::showEntry(int index) {
    if (index < 0 || index >= m_history.size())
        return;
    const QVariantMap entry = m_history.at(index).toMap();
    m_debounce.stop();
    m_pendingText.clear();
    m_selection = entry.value("text").toString();
    m_translation = entry.value("translation").toString();
    m_detected = entry.value("from").toString();
    m_error.clear();
    setStatus(QStringLiteral("done"));
    emit changed();
}

void Backend::removeEntry(int index) {
    if (index < 0 || index >= m_history.size())
        return;
    pushUndo();
    m_history.removeAt(index);
    setUnsaved(!m_history.isEmpty());
    emit historyChanged();
}

void Backend::clearHistory() {
    if (m_history.isEmpty())
        return;
    pushUndo();
    m_history.clear();
    setUnsaved(false);
    emit historyChanged();
    setNotice(tr("Cleared the session's translations."));
}

void Backend::undo() {
    if (m_undo.isEmpty()) {
        setNotice(tr("Nothing to undo."));
        return;
    }
    m_history = m_undo.takeLast();
    setUnsaved(!m_history.isEmpty());
    emit historyChanged();
    setNotice(tr("Undone."));
}

void Backend::pushUndo() {
    m_undo.append(m_history);
    while (m_undo.size() > 50)
        m_undo.removeFirst();
}

void Backend::setSourceLanguage(const QString &code) {
    if (code != QLatin1String("auto") && !isSupportedLanguage(code))
        return;
    if (code == m_source)
        return;
    m_source = code;
    saveSelection();
    emit changed();
    if (!m_selection.isEmpty())
        startTranslation();
}

void Backend::setTargetLanguage(const QString &code) {
    if (!isSupportedLanguage(code) || code == m_target)
        return;
    m_target = code;
    saveSelection();
    emit changed();
    if (!m_selection.isEmpty())
        startTranslation();
}

void Backend::setAutoTranslate(bool enabled) {
    if (enabled == m_autoTranslate)
        return;
    m_autoTranslate = enabled;
    saveSelection();
    emit changed();
    if (enabled && !m_selection.isEmpty() && m_translation.isEmpty())
        startTranslation();
}

void Backend::swapLanguages() {
    if (m_source == QLatin1String("auto") || !isSupportedLanguage(m_source))
        return;
    const QString source = m_source;
    m_source = m_target;
    m_target = source;
    saveSelection();
    emit changed();
    if (!m_selection.isEmpty())
        startTranslation();
}

void Backend::saveSelection() {
    if (!m_settings)
        return;
    m_settings->setValue("sourceLanguage", m_source);
    m_settings->setValue("targetLanguage", m_target);
    m_settings->setValue("autoTranslate", m_autoTranslate);
    m_settings->sync();
}

void Backend::setStatus(const QString &status) {
    if (m_status == status)
        return;
    m_status = status;
    emit changed();
}

void Backend::setNotice(const QString &notice) {
    if (m_notice == notice)
        return;
    m_notice = notice;
    emit noticeChanged();
    if (!m_notice.isEmpty())
        m_noticeTimer.start();
}

void Backend::setUnsaved(bool unsaved) {
    if (m_unsaved == unsaved)
        return;
    m_unsaved = unsaved;
    emit unsavedChanged();
}
