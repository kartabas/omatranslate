import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A modal question over the window. ask() shows it and runs the action only
// if the user confirms.
Rectangle {
    id: confirm

    property string title: ""
    property string message: ""
    property string confirmText: "OK"
    property var action: null

    function ask(title, message, confirmText, action) {
        confirm.title = title;
        confirm.message = message;
        confirm.confirmText = confirmText;
        confirm.action = action;
        visible = true;
        confirmButton.forceActiveFocus();
    }
    function cancel() {
        visible = false;
        action = null;
    }
    function accept() {
        const run = action;
        visible = false;
        action = null;
        if (run)
            run();
    }

    visible: false
    anchors.fill: parent
    color: "#000000cc"

    MouseArea { anchors.fill: parent; onClicked: confirm.cancel() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, Math.max(column.implicitWidth, 340) + 56)
        height: column.implicitHeight + 48
        radius: theme.radius
        color: theme.darkBackground
        border.color: Qt.rgba(1, 1, 1, 0.09)

        Material.theme: Material.Dark
        Material.accent: theme.accent
        Material.roundedScale: theme.radius

        MouseArea { anchors.fill: parent }

        Column {
            id: column
            anchors.centerIn: parent
            width: parent.width - 56
            spacing: 8

            Text {
                text: confirm.title
                color: theme.foreground
                font.pixelSize: 16
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                width: parent.width
            }
            Text {
                width: parent.width
                text: confirm.message
                color: theme.muted
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                bottomPadding: 12
            }
            Row {
                anchors.right: parent.right
                spacing: 10
                BarButton {
                    text: qsTr("Cancel")
                    onClicked: confirm.cancel()
                }
                BarButton {
                    id: confirmButton
                    text: confirm.confirmText
                    primary: true
                    focus: true
                    Keys.onReturnPressed: confirm.accept()
                    Keys.onEnterPressed: confirm.accept()
                    Keys.onEscapePressed: confirm.cancel()
                    onClicked: confirm.accept()
                }
            }
        }
    }
}
