import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

// Language picker: a pill showing the current flag and name that opens a list of every language
// the app has a translation for (found at run time, so a new Weblate language appears by itself).
Item {
    id: root

    property string currentCode: "en"
    readonly property var languages: Core.uiLanguages()
    readonly property var current: {
        for (let i = 0; i < languages.length; ++i) {
            if (languages[i].code === currentCode)
                return languages[i]
        }
        return { code: currentCode, name: currentCode, englishName: "" }
    }

    signal picked(string code)

    implicitWidth: pill.implicitWidth
    implicitHeight: 44

    Rectangle {
        id: pill
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: pillRow.implicitWidth + 28
        width: implicitWidth
        height: 44
        radius: pillTap.pressed ? 14 : (popup.visible ? 16 : height / 2)
        color: popup.visible ? MD.Token.color.secondary_container
             : (pillHover.hovered ? MD.Token.color.surface_container_highest
                                  : MD.Token.color.surface_container_high)
        Behavior on radius { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
        Behavior on color { ColorAnimation { duration: AppMotion.short } }

        Row {
            id: pillRow
            anchors.centerIn: parent
            spacing: 10

            LanguageFlag {
                anchors.verticalCenter: parent.verticalCenter
                code: root.current.code
                diameter: 26
            }
            MD.Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.current.name
                typescale: MD.Token.typescale.label_large
                color: popup.visible ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface
            }
            MD.Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: MD.Token.icon.expand_more
                size: 22
                rotation: popup.visible ? 180 : 0
                color: popup.visible ? MD.Token.color.on_secondary_container : MD.Token.color.on_surface_variant
                Behavior on rotation { NumberAnimation { duration: AppMotion.medium; easing.type: Easing.OutBack } }
            }
        }

        HoverHandler { id: pillHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { id: pillTap; onTapped: popup.visible ? popup.close() : popup.open() }

        Accessible.role: Accessible.ComboBox
        Accessible.name: qsTr("Interface language")
    }

    Popup {
        id: popup
        parent: pill
        // Opens below the pill, or above it when the list would run off the window.
        property bool above: false
        y: above ? -height - 6 : pill.height + 6
        x: pill.width - width
        onAboutToShow: {
            const win = root.Window.height
            const top = pill.mapToItem(null, 0, 0).y
            above = win > 0 && top + pill.height + 6 + implicitHeight > win - 8
        }
        width: Math.max(280, pill.width)
        padding: 8
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        transformOrigin: above ? Popup.BottomRight : Popup.TopRight

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: AppMotion.short }
                NumberAnimation { property: "scale"; from: 0.92; to: 1; duration: AppMotion.medium; easing.type: Easing.OutBack }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: AppMotion.short }
        }

        background: Rectangle {
            radius: MD.Token.shape.corner.extra_large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
        }

        contentItem: ColumnLayout {
            spacing: 2

            Repeater {
                model: root.languages

                delegate: LanguageRow {
                    required property var modelData
                    Layout.fillWidth: true
                    code: modelData.code
                    name: modelData.name
                    englishName: modelData.englishName
                    selected: modelData.code === root.currentCode
                    onClicked: {
                        popup.close()
                        root.picked(modelData.code)
                    }
                }
            }
        }
    }
}
