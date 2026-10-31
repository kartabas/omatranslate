import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// The Omatranslate widget in the Omarchy bar. It owns the background switch:
// on starts the hidden service, off stops it (or keeps it open only when the
// session still has translations waiting to be exported). Everything else it
// asks the running app to do over the app's own control socket.
Panel {
  id: root
  moduleName: "kartabas.omatranslate"
  ipcTarget: "kartabas.omatranslate"
  manageIpc: false

  readonly property var shellApi: root.shell || (root.bar ? root.bar.shell : null)
  readonly property color foreground: barForeground
  readonly property color dim: Qt.darker(foreground, 1.55)
  readonly property string fontFamily: Style.font.family

  // The persisted switch. It is the widget's own shell.json entry, so it stays
  // with the bar layout and is the one thing this plugin remembers.
  readonly property bool enabled: setting("enabled", true)

  // What the last `omatranslate status --json` said. Null until it answers.
  property var status: null
  property bool acting: false

  readonly property bool running: !!status && status.running === true
  readonly property bool watching: running && status.enabled === true
  readonly property int historyCount: status ? Number(status.historyCount || 0) : 0
  readonly property string providerName: status ? String(status.providerName || "") : ""
  readonly property string endpoint: status ? String(status.endpoint || "") : ""
  readonly property bool serviceRunning: running && status.service === true

  readonly property string stateText: !root.enabled ? strings.t("panel.stateOff")
    : root.watching ? strings.t("panel.stateWatching")
    : root.running ? strings.t("panel.statePaused")
    : strings.t("panel.stateStarting")

  readonly property string tooltip: "Omatranslate — " + root.stateText

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

  function setEnabled(value) {
    if (root.enabled === value) return
    root.acting = true
    save("enabled", !!value)
    // Off asks the service to quit, but it keeps itself alive when the session
    // still has translations that have not been exported.
    run([value ? "enable" : "stop"])
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

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  onOpenedChanged: if (opened) {
    root.acting = false
    refresh()
    keys.forceActiveFocus()
  }

  // A widget restored into the bar in the "on" position should be watching
  // again after a restart, without waiting for a click.
  Component.onCompleted: {
    if (root.enabled) run(["enable"])
    refresh()
  }

  IpcHandler {
    target: root.ipcTarget
    function open(): void { root.open() }
    function close(): void { root.close() }
    function toggle(): void { root.toggle() }
    function refresh(): string { root.refresh(); return "ok" }
    function status(): string { return root.tooltip }
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
          color: root.enabled ? root.foreground : root.dim
          opacity: root.enabled ? 1 : 0.6
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
        spacing: Style.space(10)

        Text {
          width: parent.width
          text: strings.t("panel.title")
          color: root.foreground
          font.family: root.fontFamily
          font.pixelSize: Style.font.title
          font.bold: true
        }

        Text {
          width: parent.width
          text: root.stateText
          textFormat: Text.PlainText
          color: root.watching ? root.foreground : root.dim
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
        }

        Rectangle {
          width: parent.width
          height: Style.space(1)
          color: Qt.rgba(root.foreground.r, root.foreground.g, root.foreground.b, 0.10)
        }

        Item {
          width: parent.width
          implicitHeight: Math.max(toggleLabels.implicitHeight, toggle.implicitHeight)

          Column {
            id: toggleLabels
            anchors.left: parent.left
            anchors.right: toggle.left
            anchors.rightMargin: Style.space(12)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.space(2)

            Text {
              width: parent.width
              text: strings.t("panel.watch")
              color: root.foreground
              font.family: root.fontFamily
              font.pixelSize: Style.font.body
            }
            Text {
              width: parent.width
              text: strings.t("panel.watchDetail")
              color: root.dim
              font.family: root.fontFamily
              font.pixelSize: Style.font.caption
              wrapMode: Text.WordWrap
            }
          }

          ToggleSwitch {
            id: toggle
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            checked: root.enabled
            busy: root.acting
            foreground: root.foreground
            onToggled: root.setEnabled(checked)
          }
        }

        Text {
          width: parent.width
          visible: root.historyCount > 0
          text: strings.tn("panel.session", root.historyCount)
                .arg(Commons.I18n.formatNumber(root.historyCount))
          textFormat: Text.PlainText
          color: root.dim
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
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
          color: root.dim
          font.family: root.fontFamily
          font.pixelSize: Style.font.caption
          wrapMode: Text.WordWrap
        }

        RowLayout {
          width: parent.width
          spacing: Style.space(8)

          Button {
            text: strings.t("panel.open")
            bordered: true
            onClicked: root.openWindow()
          }
          Button {
            text: strings.t("panel.settings")
            bordered: true
            onClicked: root.openSettings()
          }
          Item { Layout.fillWidth: true }
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
