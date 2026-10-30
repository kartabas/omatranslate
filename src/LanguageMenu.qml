import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The language pair in the top bar. One click opens both choices; the same
// settings are also in the gear sheet. At narrow widths the button shows the
// short codes so the rest of the bar keeps its room.
Item {
    id: root

    objectName: "languageMenu"
    property bool compact: false
    readonly property bool menuOpen: menu.opened

    implicitWidth: button.implicitWidth
    implicitHeight: button.implicitHeight

    function openMenu() {
        menu.open();
    }

    function shortName(code) {
        if (code === "auto")
            return qsTr("Auto");
        return code.split("-")[0].toUpperCase();
    }

    function pairText() {
        if (compact)
            return shortName(backend.sourceLanguage) + " → " + shortName(backend.targetLanguage);
        return backend.languageName(backend.sourceLanguage) + " → "
               + backend.languageName(backend.targetLanguage);
    }

    BarButton {
        id: button
        anchors.fill: parent
        text: root.pairText()
        maxLabelWidth: root.compact ? 120 : 230
        active: menu.opened
        onClicked: menu.opened ? menu.close() : menu.open()
    }

    Popup {
        id: menu
        parent: Overlay.overlay
        width: Math.min(360, parent ? parent.width - 24 : 360)
        padding: 18

        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Material.theme: Material.Dark
        Material.accent: theme.accent
        Material.roundedScale: theme.radius

        background: Rectangle {
            color: theme.darkBackground
            radius: theme.radius
            border.color: Qt.rgba(1, 1, 1, 0.12)
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Languages")
                    color: theme.foreground
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }

                BarButton {
                    icon: "M7 8 L17 8 M14 5 L17 8 L14 11 M17 16 L7 16 M10 13 L7 16 L10 19"
                    enabled: backend.sourceLanguage !== "auto"
                    onClicked: backend.swapLanguages()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: qsTr("From")
                    color: theme.muted
                    font.pixelSize: 12
                }
                ComboBox {
                    id: sourceBox
                    Layout.fillWidth: true
                    model: backend.languages()
                    textRole: "name"
                    valueRole: "code"
                    font.pixelSize: 14
                    onActivated: backend.setSourceLanguage(currentValue)
                    Component.onCompleted: currentIndex = indexOfValue(backend.sourceLanguage)
                    Connections {
                        target: backend
                        function onChanged() {
                            sourceBox.currentIndex = sourceBox.indexOfValue(backend.sourceLanguage);
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: qsTr("Into")
                    color: theme.muted
                    font.pixelSize: 12
                }
                ComboBox {
                    id: targetBox
                    Layout.fillWidth: true
                    model: backend.languages()
                    textRole: "name"
                    valueRole: "code"
                    font.pixelSize: 14
                    onActivated: backend.setTargetLanguage(currentValue)
                    Component.onCompleted: currentIndex = indexOfValue(backend.targetLanguage)
                    Connections {
                        target: backend
                        function onChanged() {
                            targetBox.currentIndex = targetBox.indexOfValue(backend.targetLanguage);
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Selected text goes to Google Translate. Nothing is kept after you quit unless you export it.")
                color: theme.muted
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        onAboutToShow: {
            const origin = button.mapToItem(null, 0, button.height);
            x = Math.max(12, Math.min(parent.width - width - 12, origin.x));
            y = origin.y + 8;
        }
    }
}
