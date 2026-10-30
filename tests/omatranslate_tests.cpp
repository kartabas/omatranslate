#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "googletranslate.h"
#include "languages.h"
#include "selectionwatcher.h"
#include "theme.h"
#include "translator.h"

// Stands in for the portal: records what it was asked to show, and offers a
// way to answer the way a person would.
class FakePicker : public FilePicker {
    Q_OBJECT

public:
    explicit FakePicker(QObject *parent = nullptr) : FilePicker(parent) {}

    void openPdf(const QString &folder) override
    {
        openFolder = folder;
        ++openCount;
    }

    void saveMarkdown(const QUrl &suggestedUrl) override { suggestion = suggestedUrl; }

    void choosePdf(const QUrl &url) { emit pdfSelected(url); }
    void chooseSave(const QUrl &url) { emit saveSelected(url); }

    QString openFolder;
    QUrl suggestion;
    int openCount = 0;
};

// Answers at once, with a translation that shows which language it went into,
// so a test can see the pair that was used without the network.
class FakeTranslate : public GoogleTranslate {
    Q_OBJECT

public:
    struct Call {
        QString text;
        QString from;
        QString to;
    };

    void translate(const QString &text, const QString &from, const QString &to) override
    {
        calls.append({text, from, to});
        if (failNext) {
            failNext = false;
            emit failed(failure);
            return;
        }
        emit finished(QStringLiteral("[%1] %2").arg(to, text), from == QLatin1String("auto")
                                                                  ? QStringLiteral("en") : from);
    }

    QList<Call> calls;
    bool failNext = false;
    QString failure = QStringLiteral("Google refused the translation.");
};

class OmatranslateTests : public QObject {
    Q_OBJECT

private slots:
    void cleanJoinsLines()
    {
        QCOMPARE(Backend::clean(QStringLiteral("The cat\nsat on the mat.")),
                 QStringLiteral("The cat sat on the mat."));
        QCOMPARE(Backend::clean(QStringLiteral("inter-\nnational")), QStringLiteral("international"));
        QCOMPARE(Backend::clean(QStringLiteral("One.\n\nTwo.")), QStringLiteral("One.\n\nTwo."));
        QCOMPARE(Backend::clean(QStringLiteral("soft\u00ADhyphen")), QStringLiteral("softhyphen"));
        QCOMPARE(Backend::clean(QStringLiteral("  spaced   out  ")), QStringLiteral("spaced out"));
    }

    void chunkKeepsWholeSentences()
    {
        // Each sentence is long enough that two of them can't share a request,
        // so every chunk must be exactly one whole sentence.
        const QString sentence = QStringLiteral(
            "Dies ist ein Satz, der lang genug ist, um allein in eine Anfrage zu passen, "
            "und der trotzdem gut lesbar bleibt.");
        const auto chunks = GoogleTranslate::chunk(sentence + " " + sentence + " " + sentence, 200);
        QCOMPARE(chunks.size(), 3);
        for (const auto &chunk : chunks)
            QCOMPARE(chunk.text, sentence);
        QCOMPARE(chunks.at(0).join, QString());
        QCOMPARE(chunks.at(1).join, QStringLiteral(" "));
        QCOMPARE(chunks.at(2).join, QStringLiteral(" "));
    }

    void chunkKeepsParagraphsApart()
    {
        const auto chunks = GoogleTranslate::chunk(QStringLiteral("One.\n\nTwo."), 100);
        QCOMPARE(chunks.size(), 2);
        QCOMPARE(chunks.at(1).join, QStringLiteral("\n\n"));
    }

    void chunkAgreesWithItsLimit()
    {
        QString text;
        for (int i = 0; i < 200; ++i)
            text += QStringLiteral("This is sentence number %1. ").arg(i);
        const auto chunks = GoogleTranslate::chunk(text, 200);
        QVERIFY(chunks.size() > 1);
        for (const auto &chunk : chunks) {
            QVERIFY2(chunk.text.size() <= 200,
                     qPrintable(QStringLiteral("chunk of %1 characters").arg(chunk.text.size())));
            QVERIFY(!chunk.text.isEmpty());
        }
    }

    void parseReadsTheArrayShape()
    {
        QString detected, error;
        QCOMPARE(GoogleTranslate::parse(R"([["Hallo","en"]])", &detected, &error),
                 QStringLiteral("Hallo"));
        QCOMPARE(detected, QStringLiteral("en"));
        QVERIFY(error.isEmpty());
    }

    void parseReadsTheSentenceShape()
    {
        QString detected, error;
        QCOMPARE(GoogleTranslate::parse(
                     R"({"sentences":[{"trans":"Hallo"},{"trans":"Welt."}],"src":"en"})",
                     &detected, &error),
                 QStringLiteral("Hallo Welt."));
        QCOMPARE(detected, QStringLiteral("en"));
    }

    void parseRejectsTheBlockPage()
    {
        QString detected, error;
        QVERIFY(GoogleTranslate::parse("<html>Sorry…</html>", &detected, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void parseRejectsGarbage()
    {
        QString detected, error;
        QVERIFY(GoogleTranslate::parse("[", &detected, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void translatesASelection()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        Backend backend(&picker, &translate, nullptr, &settings, "de");
        backend.setDebounceInterval(0);

        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTRY_COMPARE(backend.status(), QStringLiteral("done"));
        QCOMPARE(backend.translation(), QStringLiteral("[de] Erster Satz."));
        QCOMPARE(backend.detected(), QStringLiteral("en"));
        QCOMPARE(backend.historyCount(), 1);
        QCOMPARE(translate.calls.size(), 1);
        QCOMPARE(translate.calls.first().from, QStringLiteral("auto"));
        QCOMPARE(translate.calls.first().to, QStringLiteral("de"));
    }

    void autoTranslateOffWaitsForEnter()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        Backend backend(&picker, &translate, nullptr, &settings, "de");
        backend.setAutoTranslate(false);
        backend.setDebounceInterval(0);

        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTest::qWait(50);
        QCOMPARE(translate.calls.size(), 0);
        QCOMPARE(backend.status(), QStringLiteral("idle"));

        backend.translateNow();
        QCOMPARE(backend.translation(), QStringLiteral("[de] Erster Satz."));
        QCOMPARE(backend.historyCount(), 1);
    }

    void aFailureIsReported()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        translate.failNext = true;
        Backend backend(&picker, &translate, nullptr, &settings, "de");
        backend.setDebounceInterval(0);

        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTRY_COMPARE(backend.status(), QStringLiteral("error"));
        QCOMPARE(backend.error(), translate.failure);
        QCOMPARE(backend.historyCount(), 0);
        QCOMPARE(backend.selection(), QStringLiteral("Erster Satz."));
    }

    void retryForgetsTheFailure()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        translate.failNext = true;
        Backend backend(&picker, &translate, nullptr, &settings, "de");
        backend.setDebounceInterval(0);

        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTRY_COMPARE(backend.status(), QStringLiteral("error"));
        backend.retry();
        QCOMPARE(backend.status(), QStringLiteral("done"));
        QVERIFY(backend.error().isEmpty());
        QCOMPARE(backend.historyCount(), 1);
    }

    void historyDedupesTheSamePassage()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        Backend backend(&picker, &translate, nullptr, &settings, "de");
        backend.setDebounceInterval(0);

        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTRY_COMPARE(backend.historyCount(), 1);
        backend.handleSelection(QStringLiteral("Zweiter Satz."));
        QTRY_COMPARE(backend.historyCount(), 2);
        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTRY_COMPARE(backend.history().first().toMap().value("text").toString(),
                     QStringLiteral("Erster Satz."));
        QCOMPARE(backend.historyCount(), 2);
        QCOMPARE(translate.calls.size(), 2);
    }

    void changingTheTargetKeepsBoth()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        Backend backend(&picker, &translate, nullptr, &settings, "de");
        backend.setDebounceInterval(0);

        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTRY_COMPARE(backend.historyCount(), 1);
        backend.setTargetLanguage(QStringLiteral("da"));
        QCOMPARE(backend.historyCount(), 2);
        QCOMPARE(backend.history().first().toMap().value("to").toString(), QStringLiteral("da"));
    }

    void removingAndClearingCanBeUndone()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        Backend backend(&picker, &translate, nullptr, &settings, "de");
        backend.setDebounceInterval(0);

        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTRY_COMPARE(backend.historyCount(), 1);
        backend.handleSelection(QStringLiteral("Zweiter Satz."));
        QTRY_COMPARE(backend.historyCount(), 2);

        backend.removeEntry(0);
        QCOMPARE(backend.historyCount(), 1);
        backend.undo();
        QCOMPARE(backend.historyCount(), 2);

        backend.clearHistory();
        QCOMPARE(backend.historyCount(), 0);
        QVERIFY(!backend.unsaved());
        backend.undo();
        QCOMPARE(backend.historyCount(), 2);
        QVERIFY(backend.unsaved());
    }

    void exportMarkdownReadsOldestFirst()
    {
        const QVariantList history{
            // Newest first, as the session list keeps them: Welt came later.
            QVariantMap{{"text", "Welt"}, {"translation", "World"}, {"from", "de"},
                        {"to", "en"}, {"document", "paper.pdf"}, {"time", "2026-10-30T12:01:00"}},
            QVariantMap{{"text", "Hallo"}, {"translation", "Hello"}, {"from", "de"},
                        {"to", "en"}, {"document", "paper.pdf"}, {"time", "2026-10-30T12:00:00"}},
        };
        const QString markdown = Backend::exportMarkdown(history, QStringLiteral("paper.pdf"));
        QVERIFY(markdown.startsWith(QStringLiteral("# Translations\n\n")));
        QVERIFY(markdown.contains(QStringLiteral("- **Document:** paper.pdf")));
        QVERIFY(markdown.contains(QStringLiteral("## 1 · German → English")));
        QVERIFY(markdown.contains(QStringLiteral("## 2 · German → English")));
        QVERIFY(markdown.contains(QStringLiteral("> Hallo")));
        QVERIFY(markdown.contains(QStringLiteral("> Welt")));
        QVERIFY(markdown.indexOf(QStringLiteral("> Hallo")) < markdown.indexOf(QStringLiteral("> Welt")));
        QVERIFY(markdown.contains(QStringLiteral("World")));
        QVERIFY(Backend::exportMarkdown({}, QStringLiteral("paper.pdf")).isEmpty());
    }

    void exportingWritesTheChosenFile()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        settings.setValue("exportFolder", dir.path());
        FakePicker picker;
        FakeTranslate translate;
        Backend backend(&picker, &translate, nullptr, &settings, "de");
        backend.setDebounceInterval(0);

        backend.handleSelection(QStringLiteral("Erster Satz."));
        QTRY_COMPARE(backend.historyCount(), 1);
        QVERIFY(backend.unsaved());

        backend.exportTranslations();
        QVERIFY(picker.suggestion.isLocalFile());
        QCOMPARE(picker.suggestion.fileName(), QStringLiteral("omatranslate.md"));
        QCOMPARE(QFileInfo(picker.suggestion.toLocalFile()).absolutePath(), dir.path());

        picker.chooseSave(picker.suggestion);
        QVERIFY(QFile::exists(picker.suggestion.toLocalFile()));
        QFile saved(picker.suggestion.toLocalFile());
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const QString markdown = QString::fromUtf8(saved.readAll());
        QVERIFY(markdown.contains(QStringLiteral("> Erster Satz.")));
        QVERIFY(markdown.contains(QStringLiteral("[de] Erster Satz.")));
        QVERIFY(!backend.unsaved());
        QCOMPARE(settings.value("exportFolder").toString(), dir.path());
    }

    void writeAtomicallyReplacesTheWholeFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("note.md");
        QString error;
        QVERIFY(Backend::writeAtomically(path, QByteArray("first and longer"), &error));
        QVERIFY(Backend::writeAtomically(path, QByteArray("short"), &error));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("short"));
    }

    void settingsSurviveARestart()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.conf");
        FakePicker picker;
        FakeTranslate translate;
        {
            QSettings settings(path, QSettings::IniFormat);
            Backend backend(&picker, &translate, nullptr, &settings, "de");
            backend.setTargetLanguage(QStringLiteral("da"));
            backend.setAutoTranslate(false);
            settings.sync();
            QCOMPARE(settings.value("targetLanguage").toString(), QStringLiteral("da"));
        }

        QSettings settings(path, QSettings::IniFormat);
        Backend backend(&picker, &translate, nullptr, &settings, "en");
        QCOMPARE(backend.targetLanguage(), QStringLiteral("da"));
        QCOMPARE(backend.autoTranslate(), false);
        QCOMPARE(backend.sourceLanguage(), QStringLiteral("auto"));
    }

    void unknownDefaultTargetFallsBackToEnglish()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        Backend backend(&picker, &translate, nullptr, &settings, "klingon");
        QCOMPARE(backend.targetLanguage(), QStringLiteral("en"));
    }

    void openingRejectsWhatIsNotAPdf()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("settings.conf"), QSettings::IniFormat);
        FakePicker picker;
        FakeTranslate translate;
        Backend backend(&picker, &translate, nullptr, &settings, "de");

        QVERIFY(!backend.openUrl(QUrl::fromLocalFile(dir.filePath("missing.pdf"))));
        QVERIFY(!backend.notice().isEmpty());

        const QString notes = dir.filePath("notes.txt");
        write(notes, "not a pdf");
        QVERIFY(!backend.openUrl(QUrl::fromLocalFile(notes)));
        QVERIFY(backend.notice().contains(QStringLiteral("notes.txt")));
        QVERIFY(!backend.hasDocument());
    }

    void omarchyLanguagesMapToGoogles()
    {
        QCOMPARE(targetForOmarchyLanguage(QStringLiteral("de")), QStringLiteral("de"));
        QCOMPARE(targetForOmarchyLanguage(QStringLiteral("pt-BR")), QStringLiteral("pt"));
        QCOMPARE(targetForOmarchyLanguage(QStringLiteral("fil")), QStringLiteral("tl"));
        QCOMPARE(targetForOmarchyLanguage(QStringLiteral("zh-Hans")), QStringLiteral("zh-CN"));
        QCOMPARE(targetForOmarchyLanguage(QStringLiteral("klingon")), QStringLiteral("en"));
        QCOMPARE(languageName(QStringLiteral("da")), QStringLiteral("Dansk"));
        QVERIFY(isSupportedLanguage(QStringLiteral("da")));
        QVERIFY(!isSupportedLanguage(QStringLiteral("klingon")));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QStringLiteral("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QStringLiteral("black"));
        QCOMPARE(theme.background(), QStringLiteral("#1e1e2e"));
        QCOMPARE(theme.radius(), 0);
    }

    void themeReadsItsColors()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "accent = \"#112233\"\n"
              "background = \"#101010\"\n"
              "dark_background = \"#080808\"\n"
              "foreground = \"#eeeeee\"\n"
              "muted = \"#777777\"\n"
              "selection = \"#333333\"\n"
              "red = \"#cc0000\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QStringLiteral("#112233"));
        QCOMPARE(theme.accentForeground(), QStringLiteral("white"));
        QCOMPARE(theme.background(), QStringLiteral("#101010"));
        QCOMPARE(theme.darkBackground(), QStringLiteral("#080808"));
        QCOMPARE(theme.foreground(), QStringLiteral("#eeeeee"));
        QCOMPARE(theme.muted(), QStringLiteral("#777777"));
        QCOMPARE(theme.selection(), QStringLiteral("#333333"));
        QCOMPARE(theme.danger(), QStringLiteral("#cc0000"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QStringLiteral("en"));
        QVERIFY(translator.translate("Main", "Cancel").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Cancel": "Cancelar", "Close": "Fechar"})");
        write(dir.path() + "/pt-BR.json", R"({"Cancel": "Cancelar BR"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QStringLiteral("pt-BR"));
        QCOMPARE(translator.translate("Main", "Cancel"), QStringLiteral("Cancelar BR"));
        QCOMPARE(translator.translate("Main", "Close"), QStringLiteral("Fechar"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n passage": {"one": "%n passage", "other": "%n passages"}})");
        write(dir.path() + "/ru.json", R"({"%n passage": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n passage", nullptr, 21), QStringLiteral("%n клип"));
        QCOMPARE(translator.translate("Main", "%n passage", nullptr, 22), QStringLiteral("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n passage", nullptr, 25), QStringLiteral("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QStringLiteral("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QStringLiteral("other"));
    }

    void translatorReadsOnlyOneCode()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "da\nnoise\n");
        QCOMPARE(Translator::readLanguage(dir.path() + "/language"), QStringLiteral("da"));
        write(dir.path() + "/language", "<script>\n");
        QCOMPARE(Translator::readLanguage(dir.path() + "/language"), QStringLiteral("en"));
        QCOMPARE(Translator::readLanguage(dir.path() + "/missing"), QStringLiteral("en"));
    }

    void selectionWatcherSeesThePrimarySelection()
    {
        QTemporaryDir dir;
        const QString selection = dir.filePath("selection.txt");
        const QString script = dir.filePath("paste");
        write(selection, "Hallo");
        write(script, "#!/bin/sh\ncat '" + selection.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(script, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        SelectionWatcher watcher(script, 40);
        QSignalSpy spy(&watcher, &SelectionWatcher::textChanged);
        watcher.start();
        QTRY_COMPARE(watcher.text(), QStringLiteral("Hallo"));

        // The same selection again is not a change.
        QTest::qWait(120);
        QCOMPARE(spy.count(), 1);

        write(selection, "Guten Tag");
        QTRY_COMPARE(watcher.text(), QStringLiteral("Guten Tag"));
        QVERIFY(spy.count() >= 2);
        watcher.stop();
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), static_cast<qint64>(contents.size()));
    }
};

QTEST_MAIN(OmatranslateTests)
#include "omatranslate_tests.moc"
