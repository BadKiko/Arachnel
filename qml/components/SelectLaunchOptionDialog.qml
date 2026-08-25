import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

MD.Dialog {
    id: root

    parent: Overlay.overlay
    modal: true
    title: qsTr("Select launch option")

    property string gameId: ""
    property var options: []
    property string selectedOptionId: ""
    property bool rememberChoice: false

    function openForGame(id, opts) {
        gameId = id || ""
        options = opts || []
        rememberChoice = false
        selectedOptionId = ""

        if (options && options.length > 0) {
            let defaultId = options[0].id
            for (let i = 0; i < options.length; ++i) {
                if (options[i].isDefault) {
                    defaultId = options[i].id
                    break
                }
            }
            selectedOptionId = defaultId
        }
        open()
    }

    function confirm() {
        const id = root.gameId
        const optId = root.selectedOptionId
        const remember = root.rememberChoice
        root.close()
        Core.launchGameWithOption(id, optId, remember)
    }

    contentItem: ColumnLayout {
        spacing: MD.Token.spacing.medium
        width: parent ? parent.width : implicitWidth

        MD.Label {
            Layout.fillWidth: true
            text: qsTr("Choose which version or mode you want to start:")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            Repeater {
                model: root.options

                delegate: Rectangle {
                    id: optionCard
                    Layout.fillWidth: true
                    implicitHeight: optionRow.implicitHeight + MD.Token.spacing.medium * 2
                    radius: MD.Token.shape.corner.medium

                    readonly property bool isSelected: root.selectedOptionId === modelData.id

                    color: isSelected
                           ? MD.Token.color.secondary_container
                           : (optMouse.containsMouse ? MD.Token.color.surface_container_high : MD.Token.color.surface_container)

                    border.width: isSelected ? 2 : 1
                    border.color: isSelected ? MD.Token.color.primary : MD.Token.color.outline_variant

                    Behavior on color { ColorAnimation { duration: 120 } }
                    Behavior on border.color { ColorAnimation { duration: 120 } }

                    MouseArea {
                        id: optMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.selectedOptionId = modelData.id
                        onDoubleClicked: {
                            root.selectedOptionId = modelData.id
                            root.confirm()
                        }
                    }

                    RowLayout {
                        id: optionRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: MD.Token.spacing.medium
                        spacing: MD.Token.spacing.medium

                        Rectangle {
                            width: 20
                            height: 20
                            radius: 10
                            border.width: 2
                            border.color: optionCard.isSelected ? MD.Token.color.primary : MD.Token.color.outline
                            color: "transparent"
                            Layout.alignment: Qt.AlignVCenter

                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 5
                                color: MD.Token.color.primary
                                visible: optionCard.isSelected
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.title || qsTr("Default Option")
                                typescale: MD.Token.typescale.title_small
                                color: optionCard.isSelected ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface
                                font.bold: optionCard.isSelected
                                elide: Text.ElideRight
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: {
                                    const args = (modelData.arguments && modelData.arguments.length)
                                        ? " " + modelData.arguments.join(" ") : ""
                                    const exeName = (modelData.executable || "").split("/").pop().split("\\").pop()
                                    return exeName + args
                                }
                                typescale: MD.Token.typescale.body_small
                                color: MD.Token.color.on_surface_variant
                                elide: Text.ElideMiddle
                                visible: text.length > 0
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            height: MD.Token.spacing.small
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            MD.CheckBox {
                id: rememberBox
                checked: root.rememberChoice
                onToggled: root.rememberChoice = checked
            }

            MD.Label {
                Layout.fillWidth: true
                text: qsTr("Always use this option (can be changed in game settings)")
                typescale: MD.Token.typescale.body_small
                color: MD.Token.color.on_surface_variant
                wrapMode: Text.WordWrap

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        rememberBox.checked = !rememberBox.checked
                        root.rememberChoice = rememberBox.checked
                    }
                }
            }
        }
    }

    footer: Item {
        implicitHeight: footerRow.implicitHeight + MD.Token.spacing.medium

        MD.DialogButtonBox {
            id: footerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: root.close()
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Play")
                icon.name: MD.Token.icon.play_arrow
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                enabled: root.selectedOptionId.length > 0
                onClicked: root.confirm()
            }
        }
    }
}
