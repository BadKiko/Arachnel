import QtQuick

import Qcm.Material as MD

Item {
    id: root

    property string text: ""
    property bool readOnly: false

    readonly property int digitCount: 6
    readonly property string digits: String(text).replace(/[^0-9]/g, "").slice(0, digitCount)
    readonly property bool complete: digits.length === digitCount
    readonly property int cellSize: MD.Token.spacing.extra_large
    readonly property int cellGap: MD.Token.spacing.small

    signal accepted()

    implicitWidth: digitCount * cellSize + (digitCount - 1) * cellGap
    implicitHeight: cellSize

    Accessible.role: Accessible.EditableText
    Accessible.name: qsTr("Friend code")
    Accessible.readOnly: root.readOnly

    function setFromInput(value) {
        const next = String(value).replace(/[^0-9]/g, "").slice(0, digitCount)
        const wasComplete = root.text.length === digitCount
        if (input.text !== next)
            input.text = next
        if (root.text === next)
            return
        root.text = next
        if (!wasComplete && next.length === digitCount)
            root.accepted()
    }

    onDigitsChanged: {
        if (input.text !== digits)
            input.text = digits
    }

    Row {
        id: cells
        spacing: root.cellGap

        Repeater {
            model: root.digitCount

            Rectangle {
                required property int index

                width: root.cellSize
                height: root.cellSize
                radius: MD.Token.shape.corner.medium
                color: MD.Token.color.surface_container_highest
                border.width: input.activeFocus && index === Math.min(root.digits.length, root.digitCount - 1) ? 2 : 1
                border.color: input.activeFocus && index === Math.min(root.digits.length, root.digitCount - 1)
                              ? MD.Token.color.primary
                              : MD.Token.color.outline_variant

                MD.Label {
                    anchors.centerIn: parent
                    text: index < root.digits.length ? root.digits.charAt(index) : ""
                    typescale: MD.Token.typescale.title_large
                    color: MD.Token.color.on_surface
                }

                Rectangle {
                    visible: !root.readOnly && input.activeFocus
                             && root.digits.length < root.digitCount
                             && index === root.digits.length
                    width: 2
                    height: MD.Token.spacing.large
                    anchors.centerIn: parent
                    color: MD.Token.color.primary
                }
            }
        }
    }

    MD.TextInput {
        id: input
        anchors.fill: cells
        z: 1
        readOnly: root.readOnly
        color: MD.Util.transparent(MD.Token.color.on_surface, 0)
        selectedTextColor: color
        selectionColor: color
        cursorDelegate: Item {}
        inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhNoPredictiveText
        maximumLength: root.digitCount * 2
        onTextChanged: {
            if (!root.readOnly)
                root.setFromInput(text)
        }
        Keys.onReturnPressed: {
            if (root.complete)
                root.accepted()
        }
        Keys.onEnterPressed: {
            if (root.complete)
                root.accepted()
        }
    }

    MouseArea {
        anchors.fill: cells
        enabled: !root.readOnly
        cursorShape: Qt.IBeamCursor
        propagateComposedEvents: true
        onPressed: function (mouse) {
            input.forceActiveFocus()
            mouse.accepted = false
        }
    }
}
