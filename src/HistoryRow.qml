import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One translation in the session list: the translation first because that is
// what the person is scanning for, the original and where it came from below.
Rectangle {
    id: row

    property var entry: ({})
    readonly property color accent: theme.accent
    signal activated()
    signal removeRequested()

    implicitHeight: content.implicitHeight + 26
    radius: theme.radius
    color: hover.hovered ? Qt.rgba(1, 1, 1, 0.075) : Qt.rgba(1, 1, 1, 0.035)
    border.width: 1
    border.color: hover.hovered ? Qt.rgba(accent.r, accent.g, accent.b, 0.35)
                                : Qt.rgba(1, 1, 1, 0.06)

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }

    MouseArea {
        anchors.fill: parent
        anchors.rightMargin: 40
        onClicked: row.activated()
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 13
        spacing: 5

        Text {
            Layout.fillWidth: true
            text: row.entry.translation !== undefined ? row.entry.translation : ""
            color: theme.foreground
            font.pixelSize: 15
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
        Text {
            Layout.fillWidth: true
            text: row.entry.text !== undefined ? row.entry.text : ""
            color: theme.muted
            font.pixelSize: 12
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        Text {
            Layout.fillWidth: true
            text: row.meta()
            color: theme.muted
            opacity: 0.75
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    function meta() {
        const item = row.entry || {};
        const parts = [];
        parts.push(backend.languageName(item.from || backend.sourceLanguage) + " → "
                   + backend.languageName(item.to || backend.targetLanguage));
        if (item.document)
            parts.push(item.document);
        if (item.time) {
            // Re-render the time when Omarchy's language changes.
            translator.language;
            const time = new Date(item.time);
            if (!isNaN(time.getTime()))
                parts.push(time.toLocaleTimeString(Qt.locale(), Locale.ShortFormat));
        }
        return parts.join(" · ");
    }

    Item {
        width: 34
        height: 34
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 6

        Text {
            anchors.centerIn: parent
            text: "✕"
            color: removeHover.hovered ? theme.danger : theme.muted
            font.pixelSize: 13
        }
        HoverHandler { id: removeHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: row.removeRequested() }
    }
}
