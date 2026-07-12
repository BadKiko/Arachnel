import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Popup {
    id: root

    property Item anchor: null

    width: 360
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openAt(item) {
        anchor = item
        if (item && item.parent) {
            parent = item.parent
            x = Math.max(8, item.x + item.width - width)
            y = item.y + item.height + 8
        }
        open()
        Core.markNotificationsRead()
    }

    background: MD.ElevationRectangle {
        radius: MD.Token.shape.corner.large
        color: MD.Token.color.surface_container_high
        elevation: MD.Token.elevation.level3
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Уведомления")
                typescale: MD.Token.typescale.title_medium
            }

            MD.IconButton {
                visible: Core.notifications.count > 0
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.delete
                onClicked: Core.clearNotifications()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: MD.Token.color.outline_variant
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredWidth: 360
            Layout.preferredHeight: emptyState.visible
                                         ? 180
                                         : Math.min(360, list.contentHeight + MD.Token.spacing.small)
            visible: Core.notifications.count === 0

            ColumnLayout {
                id: emptyState
                anchors.centerIn: parent
                spacing: MD.Token.spacing.small
                visible: Core.notifications.count === 0

                MD.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    name: MD.Token.icon.notifications
                    size: 32
                    color: MD.Token.color.on_surface_variant
                }

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Пока пусто")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: MD.Token.spacing.large
                    Layout.rightMargin: MD.Token.spacing.large
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Здесь появятся установки, ошибки и другие события.")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_small
                    wrapMode: Text.WordWrap
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredWidth: 360
            Layout.preferredHeight: Math.min(360, contentHeight)
            visible: Core.notifications.count > 0
            clip: true
            model: Core.notifications
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: MD.ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                required property string notificationId
                required property string message
                required property string kind
                required property string createdAt
                required property bool read

                width: list.width
                implicitHeight: row.implicitHeight + MD.Token.spacing.medium * 2
                color: read ? "transparent" : MD.Util.transparent(MD.Token.color.primary_container, 0.35)

                RowLayout {
                    id: row
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: MD.Token.spacing.medium
                    spacing: MD.Token.spacing.medium

                    MD.Icon {
                        name: kind === "error"
                              ? MD.Token.icon.error
                              : (kind === "success" ? MD.Token.icon.check_circle : MD.Token.icon.info)
                        size: 20
                        color: kind === "error"
                               ? MD.Token.color.error
                               : (kind === "success" ? MD.Token.color.primary : MD.Token.color.on_surface_variant)
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        MD.Label {
                            Layout.fillWidth: true
                            text: message
                            typescale: MD.Token.typescale.body_medium
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: formatTime(createdAt)
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.label_small
                        }
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 1
                    color: MD.Util.transparent(MD.Token.color.outline_variant, 0.65)
                }
            }
        }
    }

    function formatTime(iso) {
        if (!iso || !iso.length)
            return ""
        const d = new Date(iso)
        if (isNaN(d.getTime()))
            return ""
        return d.toLocaleTimeString(Qt.locale(), "HH:mm")
    }
}
