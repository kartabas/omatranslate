import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Language pair, translation service, and the background switch. One job, so
// there is no OK button: every change applies at once.
Popup {
    id: sheet

    parent: Overlay.overlay
    width: Math.min(460, parent ? parent.width - 32 : 460)
    height: Math.min(parent ? parent.height - 48 : 640, body.implicitHeight + 48)
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

    contentItem: Flickable {
        id: flick
        contentWidth: width
        contentHeight: body.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height

        ScrollBar.vertical: ScrollBar {
            policy: flick.contentHeight > flick.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        }

        ColumnLayout {
            id: body
            width: flick.width
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

            Text {
                Layout.fillWidth: true
                text: qsTr("Translation service")
                color: theme.foreground
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            ComboBox {
                id: providerBox
                Layout.fillWidth: true
                model: translateService.providers()
                textRole: "name"
                valueRole: "id"
                font.pixelSize: 14
                onActivated: translateService.setProvider(currentValue)
                Component.onCompleted: currentIndex = indexOfValue(translateService.provider)
                Connections {
                    target: translateService
                    function onChanged() {
                        providerBox.currentIndex = providerBox.indexOfValue(translateService.provider);
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: {
                    const rows = translateService.providers();
                    for (let i = 0; i < rows.length; ++i)
                        if (rows[i].id === translateService.provider)
                            return rows[i].detail;
                    return "";
                }
                textFormat: Text.PlainText
                color: theme.muted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: translateService.provider === "deepl"
                spacing: 4

                Text {
                    text: qsTr("DeepL API key")
                    color: theme.muted
                    font.pixelSize: 12
                }
                TextField {
                    id: deepLField
                    Layout.fillWidth: true
                    font.pixelSize: 14
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Paste your key, free keys end in :fx")
                    text: translateService.deepLKey
                    onEditingFinished: translateService.setDeepLKey(text)
                    Connections {
                        target: translateService
                        function onChanged() {
                            if (!deepLField.activeFocus)
                                deepLField.text = translateService.deepLKey;
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: translateService.deepLKey === ""
                    text: qsTr("Without a key, translations fail before they are sent.")
                    color: theme.danger
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: translateService.provider === "libretranslate"
                spacing: 4

                Text {
                    text: qsTr("Server address")
                    color: theme.muted
                    font.pixelSize: 12
                }
                TextField {
                    id: libreUrlField
                    Layout.fillWidth: true
                    font.pixelSize: 14
                    placeholderText: "https://libretranslate.com/translate"
                    text: translateService.libreUrl
                    onEditingFinished: translateService.setLibreUrl(text)
                    Connections {
                        target: translateService
                        function onChanged() {
                            if (!libreUrlField.activeFocus)
                                libreUrlField.text = translateService.libreUrl;
                        }
                    }
                }
                Text {
                    text: qsTr("API key (optional)")
                    color: theme.muted
                    font.pixelSize: 12
                }
                TextField {
                    id: libreKeyField
                    Layout.fillWidth: true
                    font.pixelSize: 14
                    echoMode: TextInput.Password
                    placeholderText: qsTr("Only if your server asks for one")
                    text: translateService.libreKey
                    onEditingFinished: translateService.setLibreKey(text)
                    Connections {
                        target: translateService
                        function onChanged() {
                            if (!libreKeyField.activeFocus)
                                libreKeyField.text = translateService.libreKey;
                        }
                    }
                }
            }

            // The exact address, what leaves the machine, and where the key
            // lives. Not a summary: the endpoint is the one the app uses.
            Text {
                Layout.fillWidth: true
                text: translateService.disclosure
                textFormat: Text.PlainText
                color: theme.muted
                font.pixelSize: 12
                lineHeight: 1.3
                wrapMode: Text.Wrap
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
                        text: qsTr("Watch for highlights")
                        color: theme.foreground
                        font.pixelSize: 14
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Off means no Translate chip. The Omarchy bar widget switches this too.")
                        color: theme.muted
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }
                Switch {
                    checked: backend.enabled
                    onToggled: backend.setEnabled(checked)
                }
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
}
