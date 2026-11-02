// omatranslate — translate what you highlight in a PDF.

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <cstdio>

#include "appcontroller.h"
#include "backend.h"
#include "cli.h"
#include "controlserver.h"
#include "languages.h"
#include "overlay.h"
#include "portalfilepicker.h"
#include "selectionwatcher.h"
#include "theme.h"
#include "translationservice.h"
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
    app.setApplicationVersion("0.2.0");

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

    // Modes: the background service, a window asking for Settings, or the
    // ordinary window. Everything else is forwarded to an app already running
    // so two copies never watch the selection at once.
    const QString mode = args.value(1);
    const bool serviceMode = mode == QLatin1String("service");
    const bool startupSettings = mode == QLatin1String("settings");
    const bool startupShow = mode == QLatin1String("show");
    const bool startupTranslate = mode == QLatin1String("translate");
    const bool startupHide = mode == QLatin1String("hide");
    if (serviceMode || startupSettings || startupShow || startupTranslate || startupHide
        || mode == QLatin1String("open"))
        args.removeAt(1);

    if (!serviceMode) {
        QString commandName;
        QJsonObject payload;
        if (startupSettings) {
            commandName = QStringLiteral("settings");
        } else if (startupShow) {
            commandName = QStringLiteral("show");
        } else if (startupTranslate) {
            commandName = QStringLiteral("translate");
        } else if (startupHide) {
            commandName = QStringLiteral("hide");
        } else {
            commandName = QStringLiteral("open");
            if (args.size() > 1)
                payload.insert(QStringLiteral("path"), args.at(1));
        }

        bool connected = false;
        QJsonObject reply = ControlServer::send(commandName, payload, 1500, &connected);
        if (!connected && ControlServer::startService(false) && ControlServer::waitForService()) {
            // The window is a view onto the one background process, so closing
            // it leaves watching and the session alive. Starting a second GUI
            // process here would watch the selection twice.
            reply = ControlServer::send(commandName, payload, 3000, &connected);
        }
        if (connected)
            return reply.value(QStringLiteral("ok")).toBool() ? 0 : 1;
    }

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "omatranslate", "omatranslate");
    PortalFilePicker picker;
    TranslationService translate(&settings);
    SelectionWatcher watcher;
    Backend backend(&picker, &translate, &watcher, &settings,
                    targetForOmarchyLanguage(translator.language()));
    AppController controller(&backend, &translate, serviceMode);

    // Launched by the bar widget: --enable starts watching straight away,
    // --paused starts the process with the chip switched off. Both persist,
    // so the next launch remembers which one was asked for.
    if (args.contains(QStringLiteral("--paused")))
        backend.setEnabled(false);
    else if (args.contains(QStringLiteral("--enable")))
        backend.setEnabled(true);

    ControlServer server;
    server.setHandler([&controller](const QString &name, const QJsonObject &arguments) {
        return controller.handle(name, arguments);
    });
    if (!server.listen()) {
        // Only one copy can own the socket. A second service would watch the
        // selection alongside the first and never be reachable, so it leaves.
        if (serviceMode)
            return 0;
        qWarning("omatranslate: couldn't open the control socket; commands from the bar won't arrive.");
    }

    QQmlApplicationEngine engine;
    // Say why the window didn't open instead of leaving an exit code behind.
    QObject::connect(&engine, &QQmlEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            fprintf(stderr, "omatranslate: %s\n", qPrintable(warning.toString()));
    });
    // Declared after the engine, so the card's window is destroyed before the
    // engine that created it.
    Overlay overlay(&backend);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("translateService", &translate);
    engine.rootContext()->setContextProperty("overlay", &overlay);
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    controller.setWindow(qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()));
    overlay.attach(&engine);

    // The service starts hidden; the window is shown by the bar, a second
    // launch, or the plugin's menu.
    if (serviceMode)
        controller.hideMain();

    if (startupSettings && !serviceMode)
        QTimer::singleShot(0, &controller, &AppController::openSettings);
    if (args.size() > 1 && !serviceMode)
        backend.openUrl(QUrl::fromUserInput(args.at(1), QDir::currentPath(), QUrl::AssumeLocalFile));

    return app.exec();
}
