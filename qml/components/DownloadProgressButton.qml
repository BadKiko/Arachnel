import QtQuick
import QtQuick.Layouts

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

    signal activated()
    signal pauseToggleRequested()

    implicitWidth: Math.max(196, labelRow.implicitWidth + 2 * MD.Token.spacing.large)
    implicitHeight: 40

    MD.ElevationRectangle {
        id: shell
        anchors.fill: parent
        radius: MD.Token.shape.corner.full
        clip: true
        elevation: MD.Token.elevation.level0

        color: root.completed || root.readyToInstall
               ? MD.Token.color.secondary_container
               : root.installFailed
                 ? MD.Util.transparent(MD.Token.color.error, 0.18)
                 : MD.Token.color.primary

        RowLayout {
            id: labelRow
            anchors.centerIn: parent
            spacing: MD.Token.spacing.small

            MD.Icon {
                name: root.installFailed
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
                size: 18
                color: root.installFailed
                       ? MD.Token.color.error
                       : root.completed || root.readyToInstall
                         ? MD.Token.color.on_secondary_container
                         : MD.Token.color.on_primary
            }

            MD.Label {
                text: root.installFailed
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
                typescale: MD.Token.typescale.label_large
                color: root.installFailed
                       ? MD.Token.color.error
                       : root.completed || root.readyToInstall
                         ? MD.Token.color.on_secondary_container
                         : MD.Token.color.on_primary
            }
        }

        Item {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 4
            visible: root.inProgress && !root.completed && !root.readyToInstall

            Rectangle {
                anchors.fill: parent
                color: MD.Util.transparent(MD.Token.color.on_primary, 0.28)
            }

            Rectangle {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: parent.width * root.fillRatio
                color: MD.Token.color.on_primary

                Behavior on width {
                    NumberAnimation {
                        duration: MD.Token.duration.short4
                        easing: MD.Token.easing.standard
                    }
                }
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
