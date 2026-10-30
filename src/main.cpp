// omatranslate — translate what you highlight in a PDF.

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QUrl>
#include <cstdio>

#include "backend.h"
#include "cli.h"
#include "googletranslate.h"
#include "languages.h"
#include "portalfilepicker.h"
#include "selectionwatcher.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // Commands draw no window, so they must not need a display. Bare
    // omatranslate prints help, as a command line tool should; launchers and
    // the README say omatranslate open.
    const QByteArray first = argc > 1 ? QByteArray(argv[1]) : QByteArray();
    const bool command = argc == 1 || isCliCommand(first) || first == "--help" || first == "-h" ||
                         first == "--version" || first == "-v";
    if (command) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // Qt's GTK platform theme opens a display of its own.
        qputenv("QT_QPA_PLATFORMTHEME", "generic");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("omatranslate");
    app.setApplicationVersion("0.1.0");

    // Matches the window to omatranslate.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("omatranslate");
    app.setWindowIcon(QIcon::fromTheme("omatranslate"));

    QStringList args = app.arguments();
    if (first == "--version" || first == "-v") {
        printf("omatranslate %s\n", qPrintable(app.applicationVersion()));
        return 0;
    }
    if (command)
        return runCli(QStringList{"omatranslate"} + (first == "--help" || first == "-h" ? QStringList{"help"} : args.mid(1)));
    if (first == "open")
        args.removeAt(1);

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "omatranslate", "omatranslate");
    PortalFilePicker picker;
    GoogleTranslate translate;
    SelectionWatcher watcher;
    Backend backend(&picker, &translate, &watcher, &settings, targetForOmarchyLanguage(translator.language()));

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    watcher.start();
    if (args.size() > 1)
        backend.openUrl(QUrl::fromUserInput(args.at(1), QDir::currentPath(), QUrl::AssumeLocalFile));

    return app.exec();
}
