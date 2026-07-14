import QtQuick
import QtQuick.Layouts
import QtQuick.Effects

import Qcm.Material as MD

Item {
    id: root

    property int progress: 0
    property bool paused: false
    property bool downloading: false
    property bool completed: false
    property bool readyToInstall: false
    property bool installFailed: false
    property bool installing: false
    property string idleText: qsTr("Download torrent")

    readonly property bool inProgress: root.downloading || root.paused
    readonly property real fillRatio: Math.max(0, Math.min(1, root.progress / 100))
    readonly property bool showProgressFill: root.inProgress && !root.completed && !root.readyToInstall
        && !root.installing
    readonly property real pillRadius: shell.height / 2

    readonly property string actionIconName: root.installing
        ? MD.Token.icon.install_desktop
        : root.installFailed
        ? MD.Token.icon.refresh
        : root.readyToInstall
          ? MD.Token.icon.install_desktop
          : root.completed
            ? MD.Token.icon.check_circle
            : root.paused
              ? MD.Token.icon.play_arrow
              : root.inProgress
                ? MD.Token.icon.pause
                : MD.Token.icon.download

    readonly property string actionText: root.installing
        ? qsTr("Installing…")
        : root.installFailed
        ? qsTr("Retry install")
        : root.readyToInstall
          ? qsTr("Install")
          : root.completed
            ? qsTr("Downloaded")
            : root.paused
              ? qsTr("Paused · %1%").arg(root.progress)
              : root.inProgress
                ? qsTr("Downloading · %1%").arg(root.progress)
                : root.idleText

    readonly property color shellColor: root.installing
        ? MD.Token.color.primary_container
        : root.completed || root.readyToInstall
        ? MD.Token.color.secondary_container
        : root.installFailed
          ? MD.Util.transparent(MD.Token.color.error, 0.18)
          : root.showProgressFill
            ? MD.Token.color.primary_container
            : MD.Token.color.primary

    readonly property color labelColor: root.installing
        ? MD.Token.color.on_primary_container
        : root.installFailed
        ? MD.Token.color.error
        : root.completed || root.readyToInstall
          ? MD.Token.color.on_secondary_container
          : root.showProgressFill
            ? MD.Token.color.on_primary_container
            : MD.Token.color.on_primary

    signal activated()
    signal pauseToggleRequested()
    signal cancelRequested()

    component ActionRow : RowLayout {
        id: row
        required property color tone
        spacing: MD.Token.spacing.small

        MD.Icon {
            name: root.actionIconName
            size: 18
            color: row.tone
        }

        MD.Label {
            text: root.actionText
            typescale: MD.Token.typescale.label_large
            color: row.tone
        }
    }

    implicitWidth: shell.width + (cancelButton.visible ? cancelButton.implicitWidth + MD.Token.spacing.small : 0)
    implicitHeight: 40

    Item {
        id: shell
        width: Math.max(196, labelRow.implicitWidth + 2 * MD.Token.spacing.large)
        height: parent.height
        clip: true

        layer.enabled: true
        layer.effect: MD.RoundClip {
            corners: MD.Util.corners(root.pillRadius)
            size: Qt.vector2d(shell.width, shell.height)
        }

        Rectangle {
            anchors.fill: parent
            color: root.shellColor
        }

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: parent.width * root.fillRatio
            visible: root.showProgressFill
            color: MD.Token.color.primary

            Behavior on width {
                NumberAnimation {
                    duration: MD.Token.duration.short4
                    easing: MD.Token.easing.standard
                }
            }
        }

        ActionRow {
            id: labelRow
            anchors.centerIn: parent
            tone: root.labelColor
        }

        Item {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: parent.width * root.fillRatio
            clip: true
            visible: root.showProgressFill

            ActionRow {
                x: labelRow.x
                y: labelRow.y
                tone: MD.Token.color.on_primary
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: root.installing ? Qt.BusyCursor : Qt.PointingHandCursor
            enabled: !root.completed && !root.installing
            onClicked: {
                if (root.inProgress)
                    root.pauseToggleRequested()
                else
                    root.activated()
            }
        }
    }

    MD.IconButton {
        id: cancelButton
        anchors.left: shell.right
        anchors.leftMargin: MD.Token.spacing.small
        anchors.verticalCenter: parent.verticalCenter
        visible: root.inProgress && !root.completed
        mdState.type: MD.Enum.IBtStandard
        icon.name: MD.Token.icon.close
        onClicked: root.cancelRequested()
    }
}
