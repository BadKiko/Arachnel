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
    contentHeight: body.implicitHeight + MD.Token.spacing.extra_large
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    SettingsColumn {
        id: body
        margin: root.contentMargin

        SettingsGroup {
            title: qsTr("Identity")
            description: qsTr("This name is shown in invites and presence.")

            SettingsRow {
                iconName: MD.Token.icon.person

                below: AppTextField {
                    Layout.fillWidth: true
                    text: root.draftDisplayName
                    placeholderText: qsTr("Display name")
                    onTextEdited: root.draftDisplayName = text
                    onEditingFinished: root.saveIdentity()
                }
            }

            SettingsRow {
                title: qsTr("Device ID: %1").arg(Core.social.deviceId)
                iconName: MD.Token.icon.devices
            }
        }

        SettingsGroup {
            title: qsTr("Relay")
            description: qsTr("Used for invites and presence.")

            SettingsRow {
                title: Core.social.relayStatus
                titleColor: Core.social.relayConnected ? MD.Token.color.on_surface : MD.Token.color.error
                iconName: Core.social.relayConnected ? MD.Token.icon.cloud_done : MD.Token.icon.cloud_off

                below: [
                    AppTextField {
                        Layout.fillWidth: true
                        text: root.draftRelayUrl
                        placeholderText: qsTr("Relay URL")
                        onTextEdited: root.draftRelayUrl = text
                        onEditingFinished: root.saveRelay()
                    },
                    RowLayout {
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            text: qsTr("Save")
                            icon.name: MD.Token.icon.save
                            mdState.type: MD.Enum.BtFilledTonal
                            onClicked: root.saveRelay()
                        }

                        MD.Button {
                            text: qsTr("Refresh")
                            icon.name: MD.Token.icon.refresh
                            mdState.type: MD.Enum.BtText
                            onClicked: Core.refreshFriends()
                        }
                    }
                ]
            }
        }
    }
}
