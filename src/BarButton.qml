import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// A top-bar button: optional stroked icon, a label, and the key that does the
// same thing, so the shortcuts teach themselves. Follows Signed (MIT, David
// Heinemeier Hansson).
Rectangle {
    id: button

    property string text: ""
    property string icon: ""        // SVG path data on a 24×24 grid
    property string key: ""
    property bool primary: false
    property bool active: false
    property int maxLabelWidth: 260
    property color accent: theme.accent
    property color accentForeground: theme.accentForeground
    signal clicked()

    // The text as wide as it wants to be, which is what the button's natural
    // width is built from. A hidden copy measures it, so narrowing a squeezed
    // button never shrinks its own natural width.
    readonly property real naturalLabel: Math.min(measure.implicitWidth, maxLabelWidth)
    readonly property real naturalWidth: 24
        + (icon !== "" ? 16 : 0)
        + (naturalLabel > 0 ? (icon !== "" ? 8 : 0) + naturalLabel : 0)
        + (key !== "" ? ((icon !== "" || naturalLabel > 0) ? 8 : 0) + keyLabel.implicitWidth : 0)

    implicitWidth: Math.max(36, naturalWidth)
    implicitHeight: 36
    radius: theme.radius
    color: primary ? accent : (hover.hovered && enabled ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06))
    border.color: active ? accent : "transparent"
    border.width: active ? 2 : 0
    opacity: enabled ? 1 : 0.4

    // A layout may squeeze the button below its natural width (a long document
    // name in a small window), so the label gives way and elides.
    readonly property real labelRoom: Math.max(0, width - 24
        - (icon !== "" ? 24 : 0)
        - (key !== "" ? keyLabel.implicitWidth + 8 : 0))

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: button.clicked() }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8

        Shape {
            visible: button.icon !== ""
            width: 16
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                scale: Qt.size(16 / 24, 16 / 24)
                strokeColor: button.primary ? button.accentForeground : theme.foreground
                strokeWidth: 2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: button.icon }
            }
        }

        Label {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(button.naturalLabel, button.labelRoom)
            text: button.text
            textFormat: Text.PlainText
            color: button.primary ? button.accentForeground : theme.foreground
            font.pixelSize: 13
            font.weight: button.primary ? Font.DemiBold : Font.Normal
            elide: Text.ElideMiddle
        }

        Label {
            id: keyLabel
            visible: button.key !== ""
            anchors.verticalCenter: parent.verticalCenter
            text: button.key
            color: button.primary ? Qt.rgba(0, 0, 0, 0.45) : theme.muted
            font.pixelSize: 11
            font.family: "monospace"
        }
    }

    // Measures the label at its natural width; never drawn.
    Label {
        id: measure
        visible: false
        text: button.text
        textFormat: Text.PlainText
        font: label.font
    }
}
