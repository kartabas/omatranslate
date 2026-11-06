import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// The Omatranslate widget in the Omarchy bar. The mode control is the whole
// switch: Watching runs the service hidden, Paused stops the chip but keeps
// the session in memory, Stopped ends the process. Open app shows the window;
// closing it puts the app back in the background. Everything is asked of the
// running app over its own control socket.
Panel {
  id: root
  moduleName: "kartabas.omatranslate"
  ipcTarget: "kartabas.omatranslate"

  readonly property var shellApi: root.shell || (root.bar ? root.bar.shell : null)
  readonly property color foreground: barForeground
  readonly property color dim: Qt.darker(foreground, 1.55)
  readonly property string fontFamily: Style.font.family

  // The persisted switch, kept in this widget's own shell.json entry. It is
  // what starts the service again when the bar comes up.
  readonly property bool enabled: setting("enabled", true)

  // What the last `omatranslate status --json` said. Null until it answers.
  property var status: null
  property bool acting: false

  readonly property bool running: !!status && status.running === true
  readonly property bool watching: running && status.enabled === true
  readonly property int historyCount: status ? Number(status.historyCount || 0) : 0
  readonly property string providerName: status ? String(status.providerName || "") : ""

  // The three modes, in the order the control shows them.
  readonly property var modes: [
    { value: "watch", label: strings.t("panel.modeWatch") },
    { value: "pause", label: strings.t("panel.modePause") },
    { value: "stop", label: strings.t("panel.modeStop") }
  ]
  readonly property string mode: root.watching ? "watch" : root.running ? "pause" : "stop"

  readonly property string stateText: root.mode === "watch" ? strings.t("panel.stateWatching")
    : root.mode === "pause" ? (root.historyCount > 0
        ? strings.t("panel.statePaused") : strings.t("panel.statePausedOnly"))
    : root.acting ? strings.t("panel.stateStarting") : strings.t("panel.stateOff")

  readonly property string modeDetail: root.mode === "watch" ? strings.t("panel.detailWatch")
    : root.mode === "pause" ? strings.t("panel.detailPause")
    : strings.t("panel.detailStop")

  readonly property string tooltip: "Omatranslate — " + root.stateText

  // Keyboard cursor: row 0 is the mode control, row 1 the action buttons.
  property int cursorRow: 0
  property int cursorCol: 0
  readonly property var rowLengths: [root.modes.length, 2]

  Commons.Translations {
    id: strings
    directory: Qt.resolvedUrl("i18n")
  }

  function save(key, value) {
    var entry = {}
    for (var k in root.settings) if (k !== "id") entry[k] = root.settings[k]
    entry[key] = value
    root.settings = entry
    if (root.shellApi && typeof root.shellApi.updateEntryInline === "function")
      root.shellApi.updateEntryInline(root.moduleName, entry)
  }

  function run(args) {
    Quickshell.execDetached(["omatranslate"].concat(args))
  }

  function refresh() {
    if (!statusProcess.running)
      statusProcess.running = true
  }

  // Watching starts the service hidden; Paused keeps it in memory with the
  // chip switched off (starting it that way when nothing runs); Stopped ends
  // it, unless the session still has translations that were not exported.
  function setMode(mode) {
    if (mode === "watch" ? root.watching : mode === root.mode) return
    root.acting = true
    if (root.enabled !== (mode === "watch")) save("enabled", mode === "watch")
    if (mode === "watch")
      run(["enable"])
    else if (mode === "pause")
      run(root.running ? ["disable"] : ["service", "--paused"])
    else
      run(["stop"])
    settleTimer.restart()
  }

  function openWindow() {
    run(["open"])
    root.close()
  }

  function openSettings() {
    run(["settings"])
    root.close()
  }

  function moveCursor(dx, dy) {
    if (dy !== 0) {
      root.cursorRow = Math.max(0, Math.min(root.rowLengths.length - 1, root.cursorRow + dy))
      root.cursorCol = Math.min(root.cursorCol, root.rowLengths[root.cursorRow] - 1)
    } else if (dx !== 0) {
      var length = root.rowLengths[root.cursorRow]
      root.cursorCol = (root.cursorCol + dx + length) % length
    }
  }

  function activateCursor() {
    if (root.cursorRow === 0) {
      root.setMode(root.modes[root.cursorCol].value)
    } else if (root.cursorCol === 0) {
      root.openWindow()
    } else {
      root.openSettings()
    }
  }

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  onOpenedChanged: if (opened) {
    root.acting = false
    root.cursorRow = 0
    root.cursorCol = root.modes.findIndex(function (m) { return m.value === root.mode })
    refresh()
    keys.forceActiveFocus()
  }

  // A widget restored into the bar in the "on" position should be watching
  // again after a restart, without waiting for a click.
  Component.onCompleted: {
    if (root.enabled) run(["enable"])
    refresh()
  }

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    slotSize: Style.bar.statusSlot
    tooltipText: root.tooltip
    iconComponent: Component {
      Item {
        Text {
          anchors.centerIn: parent
          text: "Aa"
          color: root.watching ? root.foreground : root.dim
          opacity: root.watching ? 1 : 0.6
          font.family: root.fontFamily
          font.pixelSize: Style.font.title
          font.bold: true
        }
      }
    }
    onPressed: function(code) { if (code === Qt.LeftButton) root.toggle() }
  }

  KeyboardPanel {
    id: panel
    anchorItem: button
    owner: root
    bar: root.bar
    open: root.opened
    focusTarget: keys
    contentWidth: panel.fittedContentWidth(Style.space(360))
    contentHeight: panel.fittedContentHeight(content.implicitHeight, Style.space(460))

    PanelKeyCatcher {
      id: keys
      anchors.fill: parent
      onCloseRequested: root.close()
      onTabRequested: function(direction) { root.switchPanel(direction) }
      onMoveRequested: function(dx, dy) { root.moveCursor(dx, dy) }
      onActivateRequested: root.activateCursor()
      onTextKey: function(text, modifiers) {
        if (text === "s") root.setMode(root.mode === "watch" ? "stop" : "watch")
        else if (text === "o") root.openWindow()
        else if (text === ",") root.openSettings()
      }
    }

    Flickable {
      anchors.fill: parent
      contentWidth: width
      contentHeight: content.implicitHeight
      clip: true
      boundsBehavior: Flickable.StopAtBounds
      interactive: contentHeight > height
      ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

      Column {
        id: content
        width: parent.width
        spacing: Style.space(12)

        // ---------- Hero: mark, name, state ----------
        PanelHero {
          width: parent.width
          foreground: root.foreground
          fontFamily: root.fontFamily
          title: strings.t("panel.title")
          meta: root.stateText
          iconComponent: Component {
            Rectangle {
              width: Style.space(38)
              height: Style.space(38)
              radius: Style.cornerRadius
              color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b,
                             root.watching ? 0.10 : 0.04)

              Text {
                anchors.centerIn: parent
                text: "Aa"
                color: root.watching ? root.foreground : root.dim
                font.family: root.fontFamily
                font.pixelSize: Style.font.title
                font.bold: true
              }
            }
          }
          trailingControl: Component {
            Item {
              width: Style.space(10)
              height: Style.space(10)

              Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: root.watching ? Commons.Color.accent : root.dim
                opacity: root.watching ? 1 : 0.45

                Behavior on color { ColorAnimation { duration: Style.duration(180) } }
              }
            }
          }
        }

        // ---------- Mode: one row of three, the panel's main control ----------
        PanelSeparator {
          foreground: root.foreground
        }

        Column {
          width: parent.width
          spacing: Style.space(8)

          PanelSectionHeader {
            text: strings.t("panel.mode")
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Row {
            id: modeRow
            width: parent.width
            spacing: Style.space(6)

            readonly property real cellWidth: root.modes.length > 0
              ? (width - spacing * (root.modes.length - 1)) / root.modes.length
              : 0

            Repeater {
              model: root.modes

              Button {
                required property var modelData
                required property int index
                width: modeRow.cellWidth
                text: modelData.label
                fontSize: Style.font.bodySmall
                foreground: root.foreground
                fontFamily: root.fontFamily
                bordered: true
                selected: root.mode === modelData.value
                hasCursor: root.cursorRow === 0 && root.cursorCol === index
                onClicked: root.setMode(modelData.value)
                onHovered: function(h) {
                  if (h) {
                    root.cursorRow = 0
                    root.cursorCol = index
                  }
                }
              }
            }
          }

          Text {
            width: parent.width
            text: root.modeDetail
            textFormat: Text.PlainText
            color: root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            wrapMode: Text.WordWrap
          }
        }

        // ---------- Actions ----------
        PanelSeparator {
          foreground: root.foreground
        }

        Column {
          width: parent.width
          spacing: Style.space(8)

          PanelSectionHeader {
            text: strings.t("panel.actions")
            foreground: root.foreground
            fontFamily: root.fontFamily
          }

          Row {
            width: parent.width
            spacing: Style.space(6)

            Button {
              width: (parent.width - parent.spacing) / 2
              text: strings.t("panel.openApp")
              iconText: "▣"
              fontSize: Style.font.bodySmall
              foreground: root.foreground
              fontFamily: root.fontFamily
              bordered: true
              hasCursor: root.cursorRow === 1 && root.cursorCol === 0
              onClicked: root.openWindow()
              onHovered: function(h) {
                if (h) {
                  root.cursorRow = 1
                  root.cursorCol = 0
                }
              }
            }

            Button {
              width: (parent.width - parent.spacing) / 2
              text: strings.t("panel.settings")
              iconText: "⚙"
              fontSize: Style.font.bodySmall
              foreground: root.foreground
              fontFamily: root.fontFamily
              bordered: true
              hasCursor: root.cursorRow === 1 && root.cursorCol === 1
              onClicked: root.openSettings()
              onHovered: function(h) {
                if (h) {
                  root.cursorRow = 1
                  root.cursorCol = 1
                }
              }
            }
          }
        }

        // ---------- This session ----------
        PanelSeparator {
          foreground: root.foreground
        }

        Column {
          width: parent.width
          spacing: Style.space(4)

          Text {
            width: parent.width
            text: root.historyCount > 0
                  ? strings.tn("panel.session", root.historyCount)
                        .arg(Commons.I18n.formatNumber(root.historyCount))
                  : strings.t("panel.sessionEmpty")
            textFormat: Text.PlainText
            color: root.historyCount > 0 ? root.foreground : root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.bodySmall
            wrapMode: Text.WordWrap
          }

          Text {
            width: parent.width
            visible: root.providerName !== ""
            text: root.providerName !== ""
                  ? strings.t("panel.provider").arg(root.providerName)
                  : ""
            textFormat: Text.PlainText
            color: root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            wrapMode: Text.WordWrap
          }

          Text {
            width: parent.width
            text: strings.t("panel.manual")
            textFormat: Text.PlainText
            color: root.dim
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            wrapMode: Text.WordWrap
          }
        }
      }
    }
  }

  Process {
    id: statusProcess
    command: ["omatranslate", "status", "--json"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        try {
          var parsed = JSON.parse(text)
          root.status = (parsed && parsed.running !== undefined) ? parsed : null
        } catch (e) {
          root.status = null
        }
      }
    }
    onExited: function(code) {
      if (code !== 0) root.status = null
      root.acting = false
    }
  }

  Timer {
    id: settleTimer
    interval: 700
    repeat: false
    onTriggered: root.refresh()
  }

  // While the panel is open the session can change under it (the window
  // exports or clears), so keep the readout honest.
  Timer {
    interval: 5000
    running: root.opened
    repeat: true
    onTriggered: root.refresh()
  }
}
