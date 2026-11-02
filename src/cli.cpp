#include "cli.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <cstdio>

#include "controlserver.h"
#include "languages.h"
#include "translationservice.h"

namespace {

const QString PluginId = QStringLiteral("kartabas.omatranslate");

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

QJsonObject statusReply(int timeout = 1200) {
    bool connected = false;
    QJsonObject reply = ControlServer::send(QStringLiteral("status"), {}, timeout, &connected);
    if (connected) {
        reply.insert(QStringLiteral("running"), true);
        return reply;
    }
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "omatranslate", "omatranslate");
    QJsonObject offline;
    offline.insert(QStringLiteral("ok"), true);
    offline.insert(QStringLiteral("running"), false);
    offline.insert(QStringLiteral("service"), false);
    offline.insert(QStringLiteral("enabled"), settings.value("enabled", true).toBool());
    offline.insert(QStringLiteral("historyCount"), 0);
    offline.insert(QStringLiteral("version"), QCoreApplication::applicationVersion());
    return offline;
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

    // The command uses whichever service is chosen in Settings, so a script
    // and the window never disagree about where text is sent.
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "omatranslate", "omatranslate");
    TranslationService service(&settings);
    QEventLoop loop;
    QTimer timeout;
    QString translation, detected, error;
    QObject::connect(&service, &TranslationService::finished, &loop,
                     [&](const QString &result, const QString &language) {
        translation = result;
        detected = language;
        loop.quit();
    });
    QObject::connect(&service, &TranslationService::failed, &loop, [&](const QString &message) {
        error = message;
        loop.quit();
    });
    timeout.setSingleShot(true);
    timeout.setInterval(30000);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] {
        error = service.providerName() + " didn't answer within 30 seconds.";
        service.cancel();
        loop.quit();
    });
    timeout.start();
    service.translate(text, from, to);
    loop.exec();

    if (!error.isEmpty())
        return fail(error);

    if (parser.isSet("json"))
        print(QJsonObject{{"translation", translation}, {"detected", detected}, {"target", to},
                          {"provider", service.provider()}});
    else
        print(stdout, translation);
    return 0;
}

int languages(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("List the language codes the translators accept.");
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

int status(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Report whether Omatranslate is running and watching.");
    parser.addHelpOption();
    parser.addOption({"json", "Print the status as JSON"});
    parser.process(arguments);

    const QJsonObject reply = statusReply();
    if (parser.isSet("json")) {
        print(reply);
        return 0;
    }
    if (!reply.value("running").toBool()) {
        print(stdout, reply.value("enabled").toBool()
                           ? QStringLiteral("Omatranslate isn't running (it will watch when started).")
                           : QStringLiteral("Omatranslate isn't running and is switched off."));
        return 0;
    }
    const bool enabled = reply.value("enabled").toBool();
    QString line = enabled ? QStringLiteral("Omatranslate is on.") : QStringLiteral("Omatranslate is paused.");
    if (reply.value("service").toBool())
        line += QStringLiteral(" Running in the background.");
    print(stdout, line);
    return 0;
}

int enable(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Start Omatranslate in the background and watch for highlights.");
    parser.addHelpOption();
    parser.addOption({"json", "Print the result as JSON"});
    parser.process(arguments);

    bool connected = false;
    QJsonObject reply = ControlServer::send(QStringLiteral("enable"), {}, 600, &connected);
    bool started = false;
    if (!connected) {
        // The command itself runs without a display, so its own environment
        // says offscreen. The service must not inherit that: it is the process
        // that shows the window.
        QProcess process;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("QT_QPA_PLATFORM"));
        environment.remove(QStringLiteral("QT_QPA_PLATFORMTHEME"));
        process.setProcessEnvironment(environment);
        process.setProgram(QCoreApplication::applicationFilePath());
        process.setArguments({QStringLiteral("service"), QStringLiteral("--enable")});
        started = process.startDetached();
        if (!started)
            return fail("Couldn't start the Omatranslate service.");
        // Give the new process a moment to open its socket.
        for (int i = 0; i < 20 && !connected; ++i) {
            QThread::msleep(100);
            reply = ControlServer::send(QStringLiteral("status"), {}, 300, &connected);
        }
    }

    QJsonObject result{{"ok", true}, {"enabled", true}, {"started", started}, {"running", connected}};
    if (parser.isSet("json"))
        print(result);
    else
        print(stdout, "Omatranslate is on. Highlight a passage, then press Translate.");
    return 0;
}

void pauseWithoutService() {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "omatranslate", "omatranslate");
    settings.setValue("enabled", false);
    settings.sync();
}

int disable(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Stop watching for highlights, keeping the session open.");
    parser.addHelpOption();
    parser.addOption({"json", "Print the result as JSON"});
    parser.process(arguments);

    bool connected = false;
    const QJsonObject reply = ControlServer::send(QStringLiteral("disable"), {}, 800, &connected);
    if (!connected)
        pauseWithoutService();

    QJsonObject result{{"ok", true}, {"enabled", false}, {"running", connected},
                       {"keptOpen", reply.value("keptOpen").toBool()}};
    if (parser.isSet("json"))
        print(result);
    else if (result.value("keptOpen").toBool())
        print(stdout, "Omatranslate is paused. The session is kept open so you can export it.");
    else
        print(stdout, "Omatranslate is paused.");
    return 0;
}

int stop(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Turn Omatranslate off, quitting unless a session is waiting to be exported.");
    parser.addHelpOption();
    parser.addOption({"json", "Print the result as JSON"});
    parser.process(arguments);

    bool connected = false;
    const QJsonObject reply = ControlServer::send(QStringLiteral("stop"), {}, 1000, &connected);
    if (!connected)
        pauseWithoutService();

    const bool keptOpen = reply.value("keptOpen").toBool();
    QJsonObject result{{"ok", true}, {"enabled", false}, {"running", connected && keptOpen},
                       {"stopped", connected && !keptOpen}, {"keptOpen", keptOpen}};
    if (parser.isSet("json"))
        print(result);
    else if (keptOpen)
        print(stdout, "Omatranslate is paused. The session is kept open so you can export it first.");
    else
        print(stdout, "Omatranslate is off.");
    return 0;
}

int quit(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Quit Omatranslate, including any running service.");
    parser.addHelpOption();
    parser.addOption({"json", "Print the result as JSON"});
    parser.process(arguments);

    bool connected = false;
    ControlServer::send(QStringLiteral("quit"), {}, 800, &connected);
    if (parser.isSet("json"))
        print(QJsonObject{{"ok", true}, {"wasRunning", connected}});
    else
        print(stdout, connected ? "Omatranslate has quit." : "Omatranslate wasn't running.");
    return 0;
}

QString pluginFolder() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/omarchy/plugins/") + PluginId;
}

QString shellConfigPath() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/omarchy/shell.json");
}

bool readShellConfig(QJsonObject *root, QString *error) {
    QFile file(shellConfigPath());
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = parseError.errorString();
        return false;
    }
    *root = document.object();
    return true;
}

bool writeShellConfig(const QJsonObject &root, QString *error) {
    QSaveFile file(shellConfigPath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

// Adds or removes the bar entry while leaving every other widget where it was.
bool setBarEntry(bool present, QString *error) {
    QJsonObject root;
    if (!readShellConfig(&root, error))
        return false;
    QJsonObject bar = root.value(QStringLiteral("bar")).toObject();
    QJsonObject layout = bar.value(QStringLiteral("layout")).toObject();
    const QStringList sections{QStringLiteral("left"), QStringLiteral("center"),
                               QStringLiteral("centerBegin"), QStringLiteral("centerEnd"),
                               QStringLiteral("right"), QStringLiteral("tray")};

    for (const QString &section : sections) {
        QJsonArray entries = layout.value(section).toArray();
        for (int i = entries.size() - 1; i >= 0; --i) {
            if (entries.at(i).toObject().value(QStringLiteral("id")).toString() == PluginId)
                entries.removeAt(i);
        }
        layout.insert(section, entries);
    }
    if (present) {
        QJsonArray entries = layout.value(QStringLiteral("centerEnd")).toArray();
        entries.append(QJsonObject{{QStringLiteral("id"), PluginId}, {QStringLiteral("enabled"), true}});
        layout.insert(QStringLiteral("centerEnd"), entries);
    }

    bar.insert(QStringLiteral("layout"), layout);
    root.insert(QStringLiteral("bar"), bar);
    return writeShellConfig(root, error);
}

int plugin(const QStringList &arguments) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Install the Omatranslate widget into the Omarchy bar.");
    parser.addHelpOption();
    parser.addPositionalArgument("action", "install, remove, or nothing to report.", "[install|remove]");
    parser.addOption({"json", "Print the result as JSON"});
    parser.process(arguments);

    const QString action = parser.positionalArguments().value(0);
    const QString folder = pluginFolder();
    const bool installed = QFileInfo::exists(folder + QStringLiteral("/manifest.json"));

    if (!action.isEmpty() && action != QLatin1String("install") && action != QLatin1String("remove"))
        return fail("Use omatranslate plugin install or omatranslate plugin remove.");

    if (action == QLatin1String("install")) {
        const QStringList files{QStringLiteral("manifest.json"), QStringLiteral("Panel.qml"),
                                QStringLiteral("i18n/en.json"), QStringLiteral("i18n/de.json"),
                                QStringLiteral("i18n/da.json")};
        for (const QString &name : files) {
            QFile source(QStringLiteral(":/omarchy/") + name);
            if (!source.open(QIODevice::ReadOnly))
                return fail("This build is missing its bar widget (" + name + ").");
            const QString target = folder + QLatin1Char('/') + name;
            QDir().mkpath(QFileInfo(target).absolutePath());
            QSaveFile destination(target);
            if (!destination.open(QIODevice::WriteOnly) || destination.write(source.readAll()) < 0
                || !destination.commit())
                return fail("Couldn't write " + target);
        }
        QString error;
        if (!setBarEntry(true, &error))
            return fail("Couldn't update " + shellConfigPath() + ": " + error);
        if (parser.isSet("json"))
            print(QJsonObject{{"ok", true}, {"installed", folder}, {"bar", shellConfigPath()}});
        else
            print(stdout, "Installed the Omatranslate widget. It is in the bar's centre-end section; "
                          "Omarchy picks it up as soon as shell.json is saved.");
        return 0;
    }

    if (action == QLatin1String("remove")) {
        if (installed && !QDir(folder).removeRecursively())
            return fail("Couldn't remove " + folder);
        QString error;
        if (!setBarEntry(false, &error))
            return fail("Couldn't update " + shellConfigPath() + ": " + error);
        if (parser.isSet("json"))
            print(QJsonObject{{"ok", true}, {"removed", folder}});
        else
            print(stdout, "Removed the Omatranslate widget from the bar.");
        return 0;
    }

    if (parser.isSet("json"))
        print(QJsonObject{{"ok", true}, {"installed", installed}, {"folder", folder},
                          {"bar", shellConfigPath()}});
    else
        print(stdout, installed ? "The Omatranslate widget is installed in " + folder
                                : "The Omatranslate widget isn't installed. Run omatranslate plugin install.");
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
           "  open [pdf]                    Open Omatranslate, opening a PDF in your reader if one is named\n"
           "  settings                      Open Omatranslate straight to Settings\n"
           "  service [--enable]            Run hidden in the background (the bar plugin starts this)\n\n"
           "Commands that need no display (omatranslate help <command> for options):\n"
           "  translate [options] <text>    Translate a passage, or - to read standard input\n"
           "  languages                     List the language codes the translators accept\n"
           "  status [--json]               Say whether the app is running, watching, or paused\n"
           "  enable                        Start watching for highlights in the background\n"
           "  disable                       Pause watching and keep the session open\n"
           "  stop                          Pause and quit unless a session is waiting to be exported\n"
           "  quit                          Quit Omatranslate, service included\n"
           "  plugin [install|remove]       Put the Omatranslate widget in the Omarchy bar\n"
           "  skill [install]               Print the skill for coding agents, or install it";
}

bool isCliCommand(const QString &word) {
    return QStringList{"translate", "languages", "status", "enable", "disable", "stop", "quit",
                       "plugin", "skill", "help"}
        .contains(word);
}

int runCli(const QStringList &arguments) {
    const QString command = arguments.value(1);
    // Each command parses a conventional argv: the program name, then its own
    // options and arguments, without the command word in between.
    const QStringList rest = QStringList{arguments.value(0)} + arguments.mid(2);
    return command == QLatin1String("translate") ? translate(rest)
         : command == QLatin1String("languages") ? languages(rest)
         : command == QLatin1String("status")    ? status(rest)
         : command == QLatin1String("enable")    ? enable(rest)
         : command == QLatin1String("disable")   ? disable(rest)
         : command == QLatin1String("stop")      ? stop(rest)
         : command == QLatin1String("quit")      ? quit(rest)
         : command == QLatin1String("plugin")    ? plugin(rest)
         : command == QLatin1String("skill")     ? skill(rest)
                                                 : help(rest);
}
