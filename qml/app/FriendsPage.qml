import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    signal openGame(string gameId)
    signal openSettings()

    readonly property int pageMargin: MD.Token.spacing.extra_large
    readonly property int cardRadius: MD.Token.shape.corner.extra_large
    readonly property bool emptyState: Core.social.friends.count === 0

    function submitInvite(code) {
        const digits = String(code).replace(/[^0-9]/g, "")
        if (digits.length === 6)
            Core.acceptFriendInvite(digits)
    }

    Item {
        anchors.fill: parent
        visible: root.emptyState

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - root.pageMargin * 2, 880)
            spacing: MD.Token.spacing.extra_large

            ColumnLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.extra_small

                MD.Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("You appear as %1").arg(Core.social.displayName)
                    typescale: MD.Token.typescale.headline_small
                }

                MD.Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Change in settings")
                    mdState.type: MD.Enum.BtText
                    onClicked: root.openSettings()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.large

                MD.ElevationRectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: Math.max(createCol.implicitHeight, addCol.implicitHeight)
                                           + MD.Token.spacing.extra_large * 2
                    radius: root.cardRadius
                    color: MD.Token.color.surface_container
                    elevation: MD.Token.elevation.level1

                    ColumnLayout {
                        id: createCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: MD.Token.spacing.extra_large
                        anchors.rightMargin: MD.Token.spacing.extra_large
                        spacing: MD.Token.spacing.medium

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: 56
                            implicitHeight: 56
                            radius: MD.Token.shape.corner.large
                            color: MD.Token.color.primary_container

                            MD.Icon {
                                anchors.centerIn: parent
                                name: MD.Token.icon.key
                                size: 28
                                color: MD.Token.color.on_primary_container
                            }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Create a friend code")
                            typescale: MD.Token.typescale.title_medium
                        }

                        FriendCodePin {
                            Layout.alignment: Qt.AlignHCenter
                            visible: (Core.social.pendingInviteCode || "").length > 0
                            text: Core.social.pendingInviteCode
                            readOnly: true
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            visible: (Core.social.pendingInviteCode || "").length === 0
                            text: qsTr("Share it with someone on another device.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Button {
                            Layout.alignment: Qt.AlignHCenter
                            text: (Core.social.pendingInviteCode || "").length
                                  ? qsTr("New code")
                                  : qsTr("Create code")
                            icon.name: MD.Token.icon.key
                            mdState.type: MD.Enum.BtFilled
                            onClicked: Core.createFriendInvite()
                        }
                    }
                }

                MD.ElevationRectangle {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: Math.max(createCol.implicitHeight, addCol.implicitHeight)
                                           + MD.Token.spacing.extra_large * 2
                    radius: root.cardRadius
                    color: MD.Token.color.surface_container
                    elevation: MD.Token.elevation.level1

                    ColumnLayout {
                        id: addCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: MD.Token.spacing.extra_large
                        anchors.rightMargin: MD.Token.spacing.extra_large
                        spacing: MD.Token.spacing.medium

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: 56
                            implicitHeight: 56
                            radius: MD.Token.shape.corner.large
                            color: MD.Token.color.secondary_container

                            MD.Icon {
                                anchors.centerIn: parent
                                name: MD.Token.icon.person_add
                                size: 28
                                color: MD.Token.color.on_secondary_container
                            }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Add a friend")
                            typescale: MD.Token.typescale.title_medium
                        }

                        FriendCodePin {
                            id: emptyAddPin
                            Layout.alignment: Qt.AlignHCenter
                            onAccepted: root.submitInvite(emptyAddPin.text)
                        }

                        MD.Button {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("Add friend")
                            icon.name: MD.Token.icon.person_add
                            mdState.type: MD.Enum.BtFilledTonal
                            enabled: emptyAddPin.complete
                            onClicked: root.submitInvite(emptyAddPin.text)
                        }
                    }
                }
            }
        }
    }

    Flickable {
        anchors.fill: parent
        visible: !root.emptyState
        contentWidth: width
        contentHeight: listCol.implicitHeight + root.pageMargin
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: listCol
            width: parent.width
            spacing: MD.Token.spacing.medium

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                Layout.topMargin: root.pageMargin
                spacing: MD.Token.spacing.medium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    MD.Label {
                        text: qsTr("Friends")
                        typescale: MD.Token.typescale.headline_small
                    }

                    MD.Label {
                        text: qsTr("You appear as %1").arg(Core.social.displayName)
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }
                }

                MD.Button {
                    text: qsTr("Create code")
                    mdState.type: MD.Enum.BtText
                    onClicked: Core.createFriendInvite()
                }
            }

            FriendCodePin {
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                visible: (Core.social.pendingInviteCode || "").length > 0
                text: Core.social.pendingInviteCode
                readOnly: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                radius: MD.Token.shape.corner.large
                color: MD.Token.color.surface_container
                border.width: 1
                border.color: MD.Token.color.outline_variant
                implicitHeight: addRow.implicitHeight + MD.Token.spacing.medium * 2

                RowLayout {
                    id: addRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: MD.Token.spacing.medium
                    spacing: MD.Token.spacing.small

                    FriendCodePin {
                        id: listAddPin
                        onAccepted: root.submitInvite(listAddPin.text)
                    }

                    MD.Button {
                        text: qsTr("Add")
                        mdState.type: MD.Enum.BtFilledTonal
                        enabled: listAddPin.complete
                        onClicked: root.submitInvite(listAddPin.text)
                    }
                }
            }

            Repeater {
                model: Core.social.friends

                Rectangle {
                    required property string friendId
                    required property string nickname
                    required property bool online
                    required property string currentGameId
                    required property string currentGameTitle
                    required property string suggestedGameId
                    required property string suggestedGameTitle
                    required property string lastSeenAt

                    Layout.fillWidth: true
                    Layout.leftMargin: root.pageMargin
                    Layout.rightMargin: root.pageMargin
                    radius: MD.Token.shape.corner.large
                    color: MD.Token.color.surface_container
                    border.width: 1
                    border.color: MD.Token.color.outline_variant
                    implicitHeight: friendRow.implicitHeight + MD.Token.spacing.medium * 2

                    RowLayout {
                        id: friendRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: MD.Token.spacing.medium
                        spacing: MD.Token.spacing.medium

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            MD.Label {
                                Layout.fillWidth: true
                                text: nickname
                                typescale: MD.Token.typescale.title_small
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: online
                                      ? ((currentGameTitle || "").length
                                         ? qsTr("Playing %1").arg(currentGameTitle)
                                         : qsTr("Online"))
                                      : qsTr("Offline")
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_small
                            }
                        }

                        MD.Button {
                            visible: (currentGameId || "").length > 0
                            text: qsTr("Open")
                            mdState.type: MD.Enum.BtText
                            onClicked: root.openGame(currentGameId)
                        }

                        MD.Button {
                            visible: (suggestedGameId || "").length > 0
                            text: qsTr("Suggestion")
                            mdState.type: MD.Enum.BtText
                            onClicked: root.openGame(suggestedGameId)
                        }

                        MD.IconButton {
                            mdState.type: MD.Enum.IBtStandard
                            icon.name: MD.Token.icon.delete
                            onClicked: Core.removeFriendById(friendId)
                        }
                    }
                }
            }
        }
    }
}
