import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The language pair and whether new highlights translate by themselves. One
// job, so there is no OK button: every change applies at once.
Popup {
    id: sheet

    parent: Overlay.overlay
    width: Math.min(440, parent ? parent.width - 32 : 440)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? Math.max(24, (parent.height - height) / 2) : 0
    padding: 24
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    background: Rectangle {
        color: theme.darkBackground
        radius: theme.radius
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 14

        Text {
            Layout.fillWidth: true
            text: qsTr("Languages")
            color: theme.foreground
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

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

            BarButton {
                id: swapButton
                Layout.alignment: Qt.AlignBottom
                icon: "M7 8 L17 8 M14 5 L17 8 L14 11 M17 16 L7 16 M10 13 L7 16 L10 19"
                enabled: backend.sourceLanguage !== "auto"
                onClicked: backend.swapLanguages()
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
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(1, 1, 1, 0.08)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: qsTr("Translate new highlights")
                    color: theme.foreground
                    font.pixelSize: 14
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Without this, press Enter to translate what you selected.")
                    color: theme.muted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
            Switch {
                checked: backend.autoTranslate
                onToggled: backend.setAutoTranslate(checked)
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Selected text goes to Google Translate. Nothing is kept after you quit unless you export it.")
            color: theme.muted
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            BarButton {
                text: qsTr("Done")
                primary: true
                onClicked: sheet.close()
            }
        }
    }
}
