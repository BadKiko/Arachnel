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
        if (digits.length !== 6)
            return
        Core.acceptFriendInvite(digits)
        emptyAddPin.text = ""
        listAddPin.text = ""
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
            spacing: MD.Token.spacing.large

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                Layout.topMargin: root.pageMargin
                spacing: MD.Token.spacing.medium

                RowLayout {
                    Layout.fillWidth: true
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

                    FriendCodePin {
                        visible: (Core.social.pendingInviteCode || "").length > 0
                        text: Core.social.pendingInviteCode
                        readOnly: true
                    }

                    MD.Button {
                        text: (Core.social.pendingInviteCode || "").length
                              ? qsTr("New code")
                              : qsTr("Create code")
                        icon.name: MD.Token.icon.key
                        mdState.type: MD.Enum.BtText
                        onClicked: Core.createFriendInvite()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    FriendCodePin {
                        id: listAddPin
                        onAccepted: root.submitInvite(listAddPin.text)
                    }

                    MD.Button {
                        text: qsTr("Add")
                        icon.name: MD.Token.icon.person_add
                        mdState.type: MD.Enum.BtFilledTonal
                        enabled: listAddPin.complete
                        onClicked: root.submitInvite(listAddPin.text)
                    }

                    Item { Layout.fillWidth: true }
                }
            }

            MD.ElevationRectangle {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                implicitHeight: friendsCol.implicitHeight + MD.Token.spacing.small * 2
                radius: MD.Token.shape.corner.large
                color: MD.Token.color.surface_container
                elevation: MD.Token.elevation.level0

                ColumnLayout {
                    id: friendsCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.small
                    spacing: 0

                    Repeater {
                        model: Core.social.friends

                        ColumnLayout {
                            id: friendItem
                            required property string friendId
                            required property string nickname
                            required property bool online
                            required property string currentGameId
                            required property string currentGameTitle
                            required property string suggestedGameId
                            required property int index

                            Layout.fillWidth: true
                            spacing: 0

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: MD.Token.spacing.small
                                Layout.rightMargin: MD.Token.spacing.extra_small
                                Layout.topMargin: MD.Token.spacing.small
                                Layout.bottomMargin: MD.Token.spacing.small
                                spacing: MD.Token.spacing.medium

                                MD.ElevationRectangle {
                                    Layout.preferredWidth: MD.Token.spacing.extra_large
                                    Layout.preferredHeight: MD.Token.spacing.extra_large
                                    radius: MD.Token.shape.corner.full
                                    color: friendItem.online ? MD.Token.color.primary_container
                                                             : MD.Token.color.surface_container_high
                                    elevation: MD.Token.elevation.level0

                                    MD.Label {
                                        anchors.centerIn: parent
                                        text: friendItem.nickname.length
                                              ? friendItem.nickname.charAt(0).toUpperCase()
                                              : "?"
                                        typescale: MD.Token.typescale.title_small
                                        color: friendItem.online ? MD.Token.color.on_primary_container
                                                                 : MD.Token.color.on_surface_variant
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    MD.Label {
                                        Layout.fillWidth: true
                                        text: friendItem.nickname
                                        elide: Text.ElideRight
                                        typescale: MD.Token.typescale.title_small
                                    }

                                    MD.Label {
                                        Layout.fillWidth: true
                                        text: friendItem.online
                                              ? ((friendItem.currentGameTitle || "").length
                                                 ? qsTr("Playing %1").arg(friendItem.currentGameTitle)
                                                 : qsTr("Online"))
                                              : qsTr("Offline")
                                        elide: Text.ElideRight
                                        color: friendItem.online ? MD.Token.color.primary
                                                                 : MD.Token.color.on_surface_variant
                                        typescale: MD.Token.typescale.body_small
                                    }
                                }

                                MD.Button {
                                    visible: (friendItem.currentGameId || "").length > 0
                                    text: qsTr("Open")
                                    mdState.type: MD.Enum.BtText
                                    onClicked: root.openGame(friendItem.currentGameId)
                                }

                                MD.IconButton {
                                    mdState.type: MD.Enum.IBtStandard
                                    icon.name: MD.Token.icon.delete
                                    onClicked: Core.removeFriendById(friendItem.friendId)
                                }
                            }

                            MD.Divider {
                                Layout.fillWidth: true
                                Layout.leftMargin: MD.Token.spacing.extra_large + MD.Token.spacing.medium
                                visible: friendItem.index < Core.social.friends.count - 1
                            }
                        }
                    }
                }
            }
        }
    }
}
