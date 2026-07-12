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
    property string idleText: qsTr("Скачать торрент")

    readonly property bool inProgress: root.downloading || root.paused
    readonly property real fillRatio: Math.max(0, Math.min(1, root.progress / 100))
    readonly property bool showProgressFill: root.inProgress && !root.completed && !root.readyToInstall
    readonly property real pillRadius: shell.height / 2

    readonly property string actionIconName: root.installFailed
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

    readonly property string actionText: root.installFailed
        ? qsTr("Повторить установку")
        : root.readyToInstall
          ? qsTr("Установить")
          : root.completed
            ? qsTr("Загружено")
            : root.paused
              ? qsTr("Пауза · %1%").arg(root.progress)
              : root.inProgress
                ? qsTr("Загрузка · %1%").arg(root.progress)
                : root.idleText

    readonly property color shellColor: root.completed || root.readyToInstall
        ? MD.Token.color.secondary_container
        : root.installFailed
          ? MD.Util.transparent(MD.Token.color.error, 0.18)
          : root.showProgressFill
            ? MD.Token.color.primary_container
            : MD.Token.color.primary

    readonly property color labelColor: root.installFailed
        ? MD.Token.color.error
        : root.completed || root.readyToInstall
          ? MD.Token.color.on_secondary_container
          : root.showProgressFill
            ? MD.Token.color.on_primary_container
            : MD.Token.color.on_primary

    signal activated()
    signal pauseToggleRequested()

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

    implicitWidth: Math.max(196, labelRow.implicitWidth + 2 * MD.Token.spacing.large)
    implicitHeight: 40

    Item {
        id: shell
        anchors.fill: parent
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
            cursorShape: Qt.PointingHandCursor
            enabled: !root.completed
            onClicked: {
                if (root.inProgress)
                    root.pauseToggleRequested()
                else
                    root.activated()
            }
        }
    }
}
