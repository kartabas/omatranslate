#pragma once

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QVariantList>

class FilePicker;
class SelectionWatcher;
class TranslationService;

// The app's one job: take the text highlighted in the PDF reader, translate it
// for the selected language pair, and keep the session's translations to
// export as a Markdown file beside the PDF. The PDF itself is never touched.
// Translation is always explicit: a highlight only offers a Translate action.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString documentPath READ documentPath NOTIFY documentChanged)
    Q_PROPERTY(QString documentName READ documentName NOTIFY documentChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentChanged)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString translation READ translation NOTIFY changed)
    Q_PROPERTY(QString detected READ detected NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(QString sourceLanguage READ sourceLanguage NOTIFY changed)
    Q_PROPERTY(QString targetLanguage READ targetLanguage NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(int historyCount READ historyCount NOTIFY historyChanged)
    Q_PROPERTY(bool unsaved READ unsaved NOTIFY unsavedChanged)
    Q_PROPERTY(QString watchError READ watchError NOTIFY changed)

public:
    Backend(FilePicker *picker, TranslationService *translate, SelectionWatcher *watcher,
            QSettings *settings = nullptr, const QString &defaultTarget = {}, QObject *parent = nullptr);

    QString documentPath() const { return m_documentPath; }
    QString documentName() const { return m_documentName; }
    bool hasDocument() const { return !m_documentPath.isEmpty(); }
    QString selection() const { return m_selection; }
    QString translation() const { return m_translation; }
    QString detected() const { return m_detected; }
    QString status() const { return m_status; }
    QString error() const { return m_error; }
    QString notice() const { return m_notice; }
    bool enabled() const { return m_enabled; }
    QString sourceLanguage() const { return m_source; }
    QString targetLanguage() const { return m_target; }
    QVariantList history() const { return m_history; }
    int historyCount() const { return m_history.size(); }
    bool unsaved() const { return m_unsaved; }
    QString watchError() const { return m_watchError; }

    // The selection as it reads well: PDF line breaks joined back into
    // sentences, hyphenated words put together, paragraphs kept apart.
    static QString clean(const QString &selection);
    static QString exportMarkdown(const QVariantList &history, const QString &documentName);
    static bool writeAtomically(const QString &path, const QByteArray &data, QString *error);

    Q_INVOKABLE QVariantList languages() const;
    Q_INVOKABLE QString languageName(const QString &code) const;

    // Tests drive the selection directly instead of through wl-paste.
    void handleSelection(const QString &text);

public slots:
    void openDocument();
    bool openUrl(const QUrl &url);
    void openInReader();
    void translateNow();
    void retry();
    void copyTranslation();
    void exportTranslations();
    void setSourceLanguage(const QString &code);
    void setTargetLanguage(const QString &code);
    void setEnabled(bool enabled);
    void swapLanguages();
    void showEntry(int index);
    void removeEntry(int index);
    void clearHistory();
    void undo();

signals:
    void changed();
    // A new highlight, as opposed to a passage picked from the session's
    // history or the current one being shown again.
    void highlighted(const QString &text);
    void enabledChanged();
    void historyChanged();
    void documentChanged();
    void noticeChanged();
    void unsavedChanged();

private:
    void setStatus(const QString &status);
    void setNotice(const QString &notice);
    void setUnsaved(bool unsaved);
    void startTranslation();
    void remember(const QString &translation, const QString &detected);
    void addHistory(const QString &text, const QString &translation, const QString &detected,
                    const QString &target, const QString &document);
    void saveTo(const QString &path);
    void pushUndo();
    void saveSelection();

    FilePicker *m_picker = nullptr;
    TranslationService *m_translate = nullptr;
    SelectionWatcher *m_watcher = nullptr;
    QSettings *m_settings = nullptr;
    QString m_source = QStringLiteral("auto");
    QString m_target = QStringLiteral("en");
    bool m_enabled = false;

    QString m_documentPath;
    QString m_documentName;
    QString m_selection;
    QString m_translation;
    QString m_detected;
    QString m_status = QStringLiteral("idle");
    QString m_error;
    QString m_notice;
    QString m_watchError;
    QVariantList m_history;
    QList<QVariantList> m_undo;
    bool m_unsaved = false;

    QString m_pendingText;
    QString m_pendingTarget;
    QString m_pendingDocument;
    int m_pendingGeneration = 0;
    QHash<QString, QString> m_cache;
    QHash<QString, QString> m_cacheDetected;

    QTimer m_noticeTimer;
};
