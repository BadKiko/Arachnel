import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property string draftDisplayName: Core.social.displayName
    property string draftRelayUrl: Core.social.relayBaseUrl

    function saveIdentity() {
        const next = draftDisplayName.trim()
        if (next.length && next !== Core.social.displayName)
            Core.social.displayName = next
    }

    function saveRelay() {
        Core.social.relayBaseUrl = draftRelayUrl.trim()
        Core.refreshFriends()
    }

    Connections {
        target: Core.social
        function onIdentityChanged() {
            root.draftDisplayName = Core.social.displayName
        }
        function onRelayBaseUrlChanged() {
            root.draftRelayUrl = Core.social.relayBaseUrl
        }
    }

    contentWidth: width
    contentHeight: body.implicitHeight + contentMargin
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: identityCol.implicitHeight + MD.Token.spacing.large * 2

            ColumnLayout {
                id: identityCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.medium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    MD.Label {
                        text: qsTr("Identity")
                        typescale: MD.Token.typescale.title_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("This name is shown in invites and presence.")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }
                }

                AppTextField {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: root.draftDisplayName
                    placeholderText: qsTr("Display name")
                    onTextEdited: root.draftDisplayName = text
                    onEditingFinished: root.saveIdentity()
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 72
                    radius: MD.Token.shape.corner.large
                    color: MD.Token.color.surface_container_high

                    MD.Label {
                        anchors.fill: parent
                        anchors.margins: MD.Token.spacing.medium
                        text: qsTr("Device ID: %1").arg(Core.social.deviceId)
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_small
                        wrapMode: Text.WordWrap
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: relayCol.implicitHeight + MD.Token.spacing.large * 2

            ColumnLayout {
                id: relayCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.large
                spacing: MD.Token.spacing.medium

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    MD.Label {
                        text: qsTr("Relay")
                        typescale: MD.Token.typescale.title_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Used for invites and presence.")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }
                }

                AppTextField {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: root.draftRelayUrl
                    placeholderText: qsTr("Relay URL")
                    onTextEdited: root.draftRelayUrl = text
                    onEditingFinished: root.saveRelay()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: MD.Token.spacing.small

                    MD.Icon {
                        name: Core.social.relayConnected ? MD.Token.icon.cloud_done
                                                        : MD.Token.icon.cloud_off
                        size: 20
                        color: Core.social.relayConnected ? MD.Token.color.primary
                                                         : MD.Token.color.error
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: Core.social.relayStatus
                        elide: Text.ElideRight
                        color: Core.social.relayConnected ? MD.Token.color.on_surface_variant
                                                         : MD.Token.color.error
                        typescale: MD.Token.typescale.body_medium
                    }

                    MD.Button {
                        text: qsTr("Save")
                        icon.name: MD.Token.icon.save
                        mdState.type: MD.Enum.BtFilled
                        onClicked: root.saveRelay()
                    }

                    MD.Button {
                        text: qsTr("Refresh")
                        icon.name: MD.Token.icon.refresh
                        mdState.type: MD.Enum.BtText
                        onClicked: Core.refreshFriends()
                    }
                }
            }
        }
    }
}
