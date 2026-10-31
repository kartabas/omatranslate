import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The card that floats over the PDF reader. It follows the desktop theme and
// never takes the keyboard away from the reader: only the pointer can reach
// it. The window itself is placed and made layer-shell by Overlay.
Window {
    id: root

    color: "transparent"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property bool chip: overlay.mode === "chip"
    readonly property bool card: overlay.mode === "card"
    property bool copied: false

    function languageLine() {
        const from = backend.detected !== "" ? backend.detected : backend.sourceLanguage;
        if (from === "auto" || from === "")
            return backend.languageName(backend.targetLanguage);
        return backend.languageName(from) + " → " + backend.languageName(backend.targetLanguage);
    }

    width: card
           ? Math.min(400, root.screen ? root.screen.width - 24 : 400)
           : chipRow.implicitWidth + 32
    height: card
            ? Math.min(cardBody.implicitHeight + 40, 470)
            : 38

    Shortcut {
        sequence: "Escape"
        context: Qt.WindowShortcut
        onActivated: overlay.dismiss()
    }

    // The invitation to translate, shown where the highlight ended.
    Rectangle {
        id: chipPanel
        anchors.fill: parent
        visible: root.chip
        radius: theme.radius > 0 ? theme.radius : 9
        color: theme.accent
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.28)

        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: "Aa"
                color: theme.accentForeground
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
            Text {
                text: qsTr("Translate")
                color: theme.accentForeground
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
        }

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: overlay.translate() }
    }

    // The passage and its translation.
    Rectangle {
        id: cardPanel
        anchors.fill: parent
        visible: root.card
        radius: theme.radius > 0 ? theme.radius : 12
        color: theme.darkBackground
        border.width: 1
        border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.55)

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 2
            color: theme.accent
            opacity: 0.8
        }

        Column {
            id: cardBody
            x: 20
            y: 20
            width: parent.width - 40
            spacing: 11

            RowLayout {
                width: parent.width
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: backend.status === "error" ? qsTr("Translation failed")
                                                     : root.languageLine()
                    textFormat: Text.PlainText
                    color: backend.status === "error" ? theme.danger : theme.accent
                    font.pixelSize: 12
                    font.letterSpacing: 0.6
                    elide: Text.ElideRight
                }

                Item {
                    width: 26
                    height: 26

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: theme.muted
                        font.pixelSize: 15
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: overlay.dismiss() }
                }
            }

            Text {
                width: parent.width
                text: backend.selection
                textFormat: Text.PlainText
                color: theme.muted
                font.pixelSize: 12
                lineHeight: 1.35
                wrapMode: Text.Wrap
                maximumLineCount: 4
                elide: Text.ElideRight
            }

            Rectangle {
                width: parent.width
                height: 1
                color: theme.accent
                opacity: 0.3
            }

            RowLayout {
                width: parent.width
                spacing: 9
                visible: backend.status === "translating"

                BusyIndicator {
                    running: parent.visible
                    implicitWidth: 20
                    implicitHeight: 20
                    padding: 0
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Translating…")
                    color: theme.muted
                    font.pixelSize: 13
                }
            }

            Flickable {
                id: translationFlick
                width: parent.width
                height: Math.min(translationText.implicitHeight, 250)
                visible: backend.translation !== ""
                contentWidth: width
                contentHeight: translationText.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentHeight > height

                ScrollBar.vertical: ScrollBar {
                    policy: translationFlick.contentHeight > translationFlick.height
                            ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                Text {
                    id: translationText
                    width: parent.width
                    text: backend.translation
                    textFormat: Text.PlainText
                    color: theme.foreground
                    font.pixelSize: 18
                    lineHeight: 1.35
                    wrapMode: Text.Wrap
                }
            }

            Text {
                width: parent.width
                visible: backend.status === "error"
                text: backend.error
                textFormat: Text.PlainText
                color: theme.danger
                font.pixelSize: 13
                lineHeight: 1.3
                wrapMode: Text.Wrap
            }

            BarButton {
                visible: backend.translation === "" && backend.status !== "translating"
                         && backend.status !== "error"
                text: qsTr("Translate")
                primary: true
                onClicked: backend.translateNow()
            }

            RowLayout {
                width: parent.width
                spacing: 8

                BarButton {
                    text: root.copied ? qsTr("Copied") : qsTr("Copy")
                    primary: true
                    enabled: backend.translation !== ""
                    onClicked: {
                        overlay.copy();
                        root.copied = true;
                        copiedTimer.restart();
                    }
                }
                BarButton {
                    visible: backend.status === "error"
                    text: qsTr("Try again")
                    onClicked: backend.retry()
                }
                Item { Layout.fillWidth: true }
                BarButton {
                    text: qsTr("Close")
                    onClicked: overlay.dismiss()
                }
            }
        }
    }

    Timer {
        id: copiedTimer
        interval: 1600
        onTriggered: root.copied = false
    }

    onVisibleChanged: {
        if (!visible) {
            root.copied = false;
            copiedTimer.stop();
        }
    }
}
