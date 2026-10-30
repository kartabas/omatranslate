#include "cli.h"

#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <cstdio>

#include "googletranslate.h"
#include "languages.h"

namespace {

void print(FILE *stream, const QString &text) { fprintf(stream, "%s\n", qPrintable(text)); }
void print(const QJsonObject &object) { fputs(QJsonDocument(object).toJson(QJsonDocument::Compact).constData(), stdout); fputc('\n', stdout); }

int fail(const QString &message) {
    print(stderr, message);
    return 1;
}

QString readStandardInput() {
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(input.readAll());
}

int translate(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Translate one passage, without a display.");
    parser.addHelpOption();
    parser.addPositionalArgument("text", "The passage to translate, or - to read standard input.", "<text>");
    parser.addOption({"from", "Source language code, or auto (default: auto).", "language", "auto"});
    parser.addOption({"to", "Target language code, such as de or pt.", "language"});
    parser.addOption({"json", "Print the result as JSON"});
    parser.process(arguments);

    const QString to = parser.value("to");
    if (to.isEmpty())
        return fail("Name a target language: omatranslate translate --to de \"Good morning\"");
    if (!isSupportedLanguage(to))
        return fail("Unknown target language " + to + ". omatranslate languages lists them.");
    const QString from = parser.value("from");
    if (from != QLatin1String("auto") && !isSupportedLanguage(from))
        return fail("Unknown source language " + from + ". omatranslate languages lists them.");

    QString text = parser.positionalArguments().join(QLatin1Char(' '));
    if (text == QLatin1String("-"))
        text = readStandardInput();
    if (text.trimmed().isEmpty())
        return fail("Nothing to translate. Name a passage, or - to read standard input.");

    GoogleTranslate translator;
    QEventLoop loop;
    QTimer timeout;
    QString translation, detected, error;
    QObject::connect(&translator, &GoogleTranslate::finished, &loop,
                     [&](const QString &result, const QString &language) {
        translation = result;
        detected = language;
        loop.quit();
    });
    QObject::connect(&translator, &GoogleTranslate::failed, &loop, [&](const QString &message) {
        error = message;
        loop.quit();
    });
    timeout.setSingleShot(true);
    timeout.setInterval(30000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        error = "Google didn't answer within 30 seconds.";
        translator.cancel();
        loop.quit();
    });
    timeout.start();
    translator.translate(text, from, to);
    loop.exec();

    if (!error.isEmpty())
        return fail(error);

    if (parser.isSet("json"))
        print(QJsonObject{{"translation", translation}, {"detected", detected}, {"target", to}});
    else
        print(stdout, translation);
    return 0;
}

int languages(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("List the language codes Google Translate accepts.");
    parser.addHelpOption();
    parser.addOption({"json", "Print the list as JSON"});
    parser.process(arguments);

    QJsonArray list;
    for (const QVariant &value : languageOptions(true)) {
        const QVariantMap language = value.toMap();
        const QString code = language.value("code").toString();
        const QString name = language.value("name").toString();
        const QString english = language.value("english").toString();
        list.append(QJsonObject{{"code", code}, {"name", name}, {"english", english}});
        if (!parser.isSet("json")) {
            if (english.isEmpty() || english == name)
                print(stdout, code + QLatin1Char('\t') + name);
            else
                print(stdout, code + QLatin1Char('\t') + name + QLatin1String(" (") + english + QLatin1Char(')'));
        }
    }
    if (parser.isSet("json")) {
        QJsonObject result;
        result.insert("languages", list);
        print(result);
    }
    return 0;
}

int skill(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Print the agent skill, or install it for your coding agents with: skill install.");
    parser.addHelpOption();
    parser.addPositionalArgument("install", "Copy to ~/.agents/skills/omatranslate and link into ~/.claude/skills",
                                 "[install]");
    parser.addOption({"json", "Print the result as JSON"});
    parser.process(arguments);

    QFile bundled(":/skill.md");
    if (!bundled.open(QIODevice::ReadOnly))
        return 1;
    const QByteArray text = bundled.readAll();

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        if (parser.isSet("json"))
            print(QJsonObject{{"skill", QString::fromUtf8(text)}});
        else
            fputs(text.constData(), stdout);
        return 0;
    }
    if (positional.first() != QLatin1String("install"))
        return fail("Use omatranslate skill to print the skill, or omatranslate skill install.");

    const QDir home = QDir::home();
    const QString directory = home.filePath(".agents/skills/omatranslate");
    QSaveFile file(directory + "/SKILL.md");
    if (!QDir().mkpath(directory) || !file.open(QIODevice::WriteOnly) || file.write(text) < 0 || !file.commit())
        return fail("Could not write " + file.fileName());
    QJsonObject result{{"installed", file.fileName()}};
    auto done = [&] {
        if (parser.isSet("json"))
            print(result);
        return 0;
    };
    if (!parser.isSet("json"))
        print(stdout, "Installed " + file.fileName());

    if (!home.exists(".claude"))
        return done();
    const QString link = home.filePath(".claude/skills/omatranslate");
    const QFileInfo existing(link);
    if (existing.isSymLink())
        QFile::remove(link);
    else if (existing.exists())
        return fail(link + " exists and is not a link; leaving it alone.");
    if (!home.mkpath(".claude/skills") || !QFile::link("../../.agents/skills/omatranslate", link))
        return fail("Could not link " + link);
    result.insert("linked", link);
    if (!parser.isSet("json"))
        print(stdout, "Linked " + link);
    return done();
}

int help(const QStringList &arguments) {
    const QString topic = arguments.value(1);
    if (isCliCommand(topic) && topic != QLatin1String("help"))
        return runCli({arguments.value(0), topic, "--help"});
    print(stdout, "Usage: omatranslate <command> [options]\n\n" + cliSummary());
    return 0;
}

}

QString cliSummary() {
    return "App:\n"
           "  open [pdf]                    Open Omatranslate, opening a PDF in your reader if one is named\n\n"
           "Commands that need no display (omatranslate help <command> for options):\n"
           "  translate [options] <text>    Translate a passage, or - to read standard input\n"
           "  languages                     List the language codes Google Translate accepts\n"
           "  skill [install]               Print the skill for coding agents, or install it";
}

bool isCliCommand(const QString &word) {
    return QStringList{"translate", "languages", "skill", "help"}.contains(word);
}

int runCli(const QStringList &arguments) {
    const QString command = arguments.value(1);
    // Each command parses a conventional argv: the program name, then its own
    // options and arguments, without the command word in between.
    const QStringList rest = QStringList{arguments.value(0)} + arguments.mid(2);
    return command == QLatin1String("translate") ? translate(rest)
         : command == QLatin1String("languages") ? languages(rest)
         : command == QLatin1String("skill")     ? skill(rest)
                                                 : help(rest);
}
