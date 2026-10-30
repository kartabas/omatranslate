import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 640
    height: 800
    minimumWidth: 420
    minimumHeight: 560
    visible: true
    title: "Omatranslate"
    color: theme.background

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property color accent: theme.accent
    readonly property bool compact: width < 560
    // Enough room for the shortcut hints and full language names.
    readonly property bool roomy: width >= 720

    // Single-key shortcuts must never fire while typing into a field, or while
    // an overlay owns the keyboard. These flags are set by the overlays
    // themselves so a shortcut never reads an object that doesn't exist yet.
    property bool helpVisible: false
    property bool settingsVisible: false
    property bool confirmVisible: false
    readonly property bool typing: activeFocusItem !== null && activeFocusItem.cursorPosition !== undefined
    readonly property bool overlayOpen: helpVisible || settingsVisible || confirmVisible
                                        || languageMenu.menuOpen
    readonly property bool keysWork: !typing && !overlayOpen

    readonly property var icons: ({
        document: "M7 3 L14 3 L18 7 L18 21 L6 21 L6 3 Z M14 3 L14 7 L18 7 M9 12 L15 12 M9 16 L15 16",
        reader: "M4 5 L10 5 Q12 5 12 7 L12 20 Q12 18 10 18 L4 18 Z M20 5 L14 5 Q12 5 12 7 L12 20 Q12 18 14 18 L20 18 Z",
        settings: "M12 9 A3 3 0 1 0 12 15 A3 3 0 1 0 12 9 M12 2.5 L12 5.5 M12 18.5 L12 21.5 M2.5 12 L5.5 12 M18.5 12 L21.5 12 M5.3 5.3 L7.4 7.4 M16.6 16.6 L18.7 18.7 M18.7 5.3 L16.6 7.4 M7.4 16.6 L5.3 18.7",
        copy: "M9 9 L20 9 L20 20 L9 20 Z M15 9 L15 4 L4 4 L4 15 L9 15",
        export: "M12 15 L12 3 M8 7 L12 3 L16 7 M4 14 L4 20 L20 20 L20 14"
    })

    property bool quitting: false

    function statusText() {
        if (backend.status === "translating")
            return qsTr("Translating…");
        if (backend.status === "waiting")
            return qsTr("Highlight detected");
        if (backend.status === "error")
            return qsTr("Translation failed");
        const from = backend.detected !== "" ? backend.languageName(backend.detected)
                   : backend.sourceLanguage !== "auto" ? backend.languageName(backend.sourceLanguage) : "";
        return from !== "" ? from + " → " + backend.languageName(backend.targetLanguage)
                           : backend.languageName(backend.targetLanguage);
    }

    function askToQuit() {
        confirm.ask(qsTr("Quit without exporting?"),
                    qsTr("Your translations are kept only for this session. Export them first if you want to keep them."),
                    qsTr("Quit"), () => {
                        win.quitting = true;
                        Qt.quit();
                    });
    }

    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        enabled: win.keysWork
        onActivated: win.helpVisible = true
    }
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        enabled: win.keysWork
        onActivated: win.close()
    }
    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        enabled: win.keysWork
        onActivated: backend.copyTranslation()
    }
    Shortcut {
        sequence: "Return"
        context: Qt.ApplicationShortcut
        enabled: win.keysWork
        onActivated: backend.translateNow()
    }
    Shortcut {
        sequence: "Ctrl+O"
        context: Qt.ApplicationShortcut
        onActivated: backend.openDocument()
    }
    Shortcut {
        sequence: "Ctrl+E"
        context: Qt.ApplicationShortcut
        onActivated: backend.exportTranslations()
    }
    Shortcut {
        sequence: "Ctrl+Z"
        context: Qt.ApplicationShortcut
        enabled: !win.settingsVisible
        onActivated: backend.undo()
    }
    Shortcut {
        sequence: "Ctrl+,"
        context: Qt.ApplicationShortcut
        onActivated: settings.open()
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: win.helpVisible
        onActivated: win.helpVisible = false
    }

    onClosing: (close) => {
        if (win.quitting || !backend.unsaved)
            return;
        close.accepted = false;
        win.askToQuit();
    }

    // The accent-tinted wash that gives the window its color, from the theme's
    // own background, so it changes when the desktop theme does.
    Rectangle {
        anchors.fill: parent
        color: theme.background
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(win.accent.r, win.accent.g, win.accent.b, 0.14) }
            GradientStop { position: 0.42; color: Qt.rgba(win.accent.r, win.accent.g, win.accent.b, 0.0) }
            GradientStop { position: 1.0; color: theme.darkBackground }
        }
    }
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 2
        color: theme.accent
        opacity: 0.75
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Item {
            Layout.fillWidth: true
            implicitHeight: 62

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: win.compact ? 14 : 22
                anchors.rightMargin: win.compact ? 10 : 18
                spacing: 8

                BarButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 70
                    Layout.maximumWidth: win.width * 0.5
                    text: backend.hasDocument ? backend.documentName : qsTr("Open PDF…")
                    icon: win.icons.document
                    key: win.roomy ? "Ctrl+O" : ""
                    onClicked: backend.openDocument()
                }

                LanguageMenu {
                    id: languageMenu
                    compact: win.width < 900
                }

                BarButton {
                    visible: !win.compact && backend.selection !== "" && backend.translation === ""
                             && backend.status !== "translating" && backend.status !== "waiting"
                             && !backend.autoTranslate
                    text: qsTr("Translate")
                    key: win.roomy ? "Enter" : ""
                    primary: true
                    onClicked: backend.translateNow()
                }

                Item { Layout.fillWidth: true }

                BarButton {
                    visible: backend.hasDocument
                    text: win.roomy ? qsTr("Reader") : ""
                    icon: win.icons.reader
                    onClicked: backend.openInReader()
                }

                BarButton {
                    icon: win.icons.settings
                    onClicked: settings.open()
                    active: win.settingsVisible
                }
            }
        }

        Flickable {
            id: scroller
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: page.height + 34
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                policy: scroller.contentHeight > scroller.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
            }

            Column {
                id: page
                width: Math.min(720, scroller.width - (win.compact ? 28 : 64))
                x: (scroller.width - width) / 2
                y: 10
                height: implicitHeight
                spacing: 18

                // Empty state: nothing highlighted yet.
                Column {
                    width: page.width
                    visible: backend.selection === ""
                    spacing: 14

                    Rectangle {
                        width: 76
                        height: 76
                        radius: Math.min(theme.radius > 0 ? 16 : 8, 16)
                        color: Qt.rgba(win.accent.r, win.accent.g, win.accent.b, 0.14)
                        border.width: 1
                        border.color: Qt.rgba(win.accent.r, win.accent.g, win.accent.b, 0.45)

                        Text {
                            anchors.centerIn: parent
                            text: "Aa"
                            color: theme.accent
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Read a PDF, in your language")
                        color: theme.foreground
                        font.pixelSize: win.compact ? 25 : 30
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Highlight a passage in your PDF reader. A Translate chip appears beside it; the translation opens right there.")
                        color: theme.muted
                        font.pixelSize: 15
                        lineHeight: 1.3
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Translating into %1.").arg(backend.languageName(backend.targetLanguage))
                        color: theme.foreground
                        opacity: 0.85
                        font.pixelSize: 14
                        wrapMode: Text.Wrap
                    }

                    Row {
                        spacing: 10
                        BarButton {
                            text: qsTr("Open a PDF…")
                            icon: win.icons.document
                            primary: true
                            onClicked: backend.openDocument()
                        }
                        BarButton {
                            text: qsTr("Choose languages")
                            icon: win.icons.settings
                            onClicked: settings.open()
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Your PDF is never changed.")
                        color: theme.muted
                        opacity: 0.8
                        font.pixelSize: 12
                    }

                    Text {
                        width: parent.width
                        visible: backend.watchError !== ""
                        text: backend.watchError
                        color: theme.danger
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }

                // The current passage and its translation.
                Rectangle {
                    width: page.width
                    height: passage.implicitHeight + 40
                    visible: backend.selection !== ""
                    radius: theme.radius
                    color: Qt.rgba(0, 0, 0, 0.22)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.08)

                    ColumnLayout {
                        id: passage
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Text {
                                Layout.fillWidth: true
                                text: win.statusText()
                                color: backend.status === "error" ? theme.danger : theme.accent
                                font.pixelSize: 12
                                font.letterSpacing: 0.8
                                elide: Text.ElideRight
                            }

                            BarButton {
                                text: qsTr("Copy")
                                key: win.compact ? "" : "Space"
                                primary: true
                                enabled: backend.translation !== ""
                                onClicked: backend.copyTranslation()
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: backend.selection
                            color: theme.muted
                            font.pixelSize: win.compact ? 13 : 14
                            lineHeight: 1.35
                            wrapMode: Text.Wrap
                            maximumLineCount: 5
                            elide: Text.ElideRight
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: theme.accent
                            opacity: 0.35
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: backend.translation !== ""
                            text: backend.translation
                            color: theme.foreground
                            font.pixelSize: win.compact ? 19 : 23
                            lineHeight: 1.35
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: backend.translation === "" && backend.status !== "error"
                            text: backend.autoTranslate ? qsTr("Translating…")
                                                        : qsTr("Click Translate beside the highlight, or press Enter.")
                            color: theme.muted
                            font.pixelSize: 15
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: backend.status === "error"
                            text: backend.error
                            color: theme.danger
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            visible: backend.status === "error"
                                     || (backend.translation === "" && !backend.autoTranslate)

                            BarButton {
                                visible: backend.status === "error"
                                text: qsTr("Try again")
                                primary: true
                                onClicked: backend.retry()
                            }
                            BarButton {
                                visible: backend.translation === "" && !backend.autoTranslate
                                         && backend.status !== "error"
                                text: qsTr("Translate")
                                key: "Enter"
                                primary: true
                                onClicked: backend.translateNow()
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                // The session's translations, newest first.
                Column {
                    width: page.width
                    spacing: 9

                    RowLayout {
                        width: parent.width
                        height: 36
                        spacing: 10

                        Text {
                            text: qsTr("This session")
                            color: theme.foreground
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            visible: backend.historyCount > 0
                            text: qsTr("%n passage", "", backend.historyCount)
                            color: theme.muted
                            font.pixelSize: 12
                        }
                        Item { Layout.fillWidth: true }

                        BarButton {
                            visible: backend.historyCount > 0
                            text: qsTr("Clear")
                            onClicked: confirm.ask(qsTr("Clear this session?"),
                                                   qsTr("The translations are removed from this window. Ctrl+Z puts them back."),
                                                   qsTr("Clear"), () => backend.clearHistory())
                        }
                        BarButton {
                            text: qsTr("Export")
                            key: win.compact ? "" : "Ctrl+E"
                            icon: win.icons.export
                            enabled: backend.historyCount > 0
                            onClicked: backend.exportTranslations()
                        }
                    }

                    Text {
                        width: parent.width
                        visible: backend.historyCount === 0
                        text: qsTr("Translations gather here for the session. Export them as Markdown when you're done.")
                        color: theme.muted
                        font.pixelSize: 12
                        lineHeight: 1.3
                        wrapMode: Text.Wrap
                        opacity: 0.85
                    }

                    Repeater {
                        model: backend.history.length
                        delegate: HistoryRow {
                            width: page.width
                            entry: backend.history[index]
                            onActivated: backend.showEntry(index)
                            onRemoveRequested: backend.removeEntry(index)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 40
            color: Qt.rgba(0, 0, 0, 0.18)
            border.width: 0

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: win.compact ? 14 : 22
                width: parent.width - (win.compact ? 28 : 44)
                text: backend.notice !== ""
                      ? backend.notice
                      : win.compact ? qsTr("Space copies · ? shortcuts")
                                    : qsTr("Space copies the translation · Ctrl+E exports · ? shows every shortcut")
                color: backend.notice !== "" ? theme.accent : theme.muted
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
    }

    SettingsPopup {
        id: settings
        onOpened: win.settingsVisible = true
        onClosed: win.settingsVisible = false
    }

    // Every shortcut in one place, so the app teaches itself.
    Rectangle {
        id: help
        anchors.fill: parent
        visible: win.helpVisible
        color: "#000000cc"

        MouseArea { anchors.fill: parent; onClicked: win.helpVisible = false }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(win.width - 40, 470)
            height: Math.min(win.height - 40, helpColumn.implicitHeight + 48)
            radius: theme.radius
            color: theme.darkBackground
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.10)

            MouseArea { anchors.fill: parent }

            Flickable {
                anchors.fill: parent
                anchors.margins: 24
                contentHeight: helpColumn.implicitHeight
                clip: true

                Column {
                    id: helpColumn
                    width: parent.width
                    spacing: 9

                    Text {
                        text: qsTr("Keyboard")
                        color: theme.foreground
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        bottomPadding: 6
                    }

                    Repeater {
                        model: [
                            { key: "Ctrl+O", what: qsTr("Open a PDF in your reader") },
                            { key: "Space", what: qsTr("Copy the translation") },
                            { key: "Enter", what: qsTr("Translate the passage now") },
                            { key: "Ctrl+E", what: qsTr("Export the session as Markdown") },
                            { key: "Ctrl+Z", what: qsTr("Undo removing translations") },
                            { key: "Ctrl+,", what: qsTr("Choose the languages") },
                            { key: "Q", what: qsTr("Quit") },
                            { key: "Esc", what: qsTr("Close this list") }
                        ]
                        delegate: Row {
                            width: helpColumn.width
                            spacing: 14

                            Text {
                                width: 74
                                text: modelData.key
                                color: theme.accent
                                font.family: "monospace"
                                font.pixelSize: 12
                            }
                            Text {
                                width: parent.width - 88
                                text: modelData.what
                                color: theme.foreground
                                font.pixelSize: 13
                                wrapMode: Text.Wrap
                            }
                        }
                    }

                    BarButton {
                        text: qsTr("Close")
                        primary: true
                        onClicked: win.helpVisible = false
                    }
                }
            }
        }
    }

    Confirm {
        id: confirm
        onVisibleChanged: win.confirmVisible = visible
    }
}
