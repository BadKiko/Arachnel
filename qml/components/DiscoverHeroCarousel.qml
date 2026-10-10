import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

// Discover's top: a Material 3 hero carousel. One wide card in focus with the game's banner,
// title and actions; the next picks wait as narrow cards beside it. Clicking a narrow card swaps
// it into focus: widths ease with a spring, the copy fades in.
Item {
    id: root

    required property var page
    required property var shelfModel

    signal openGame(string entryId)

    readonly property int maxItems: 5
    readonly property int itemCount: Math.min(shelfModel ? shelfModel.count : 0, maxItems)
    property int focusIndex: 0
    property int heroRevision: 0

    readonly property real gap: 8
    readonly property real narrowWidth: 112

    implicitHeight: itemCount > 0 ? 340 : 0
    visible: itemCount > 0

    onItemCountChanged: if (focusIndex >= itemCount) focusIndex = 0

    Connections {
        target: Core
        function onCatalogHeroCoverChanged(entryId) { root.heroRevision++ }
    }

    Row {
        anchors.fill: parent
        spacing: root.gap

        Repeater {
            model: root.shelfModel

            delegate: Item {
                id: card
                required property int index
                required property string entryId
                required property string title
                required property string coverUrl
                required property string sizeLabel

                readonly property bool shown: index < root.itemCount
                readonly property bool focused: index === root.focusIndex
                readonly property string banner: {
                    void root.heroRevision
                    const hero = Core.catalogHeroCoverUrl(card.entryId)
                    return hero && hero.length ? hero : card.coverUrl
                }

                visible: shown
                height: parent.height
                width: !shown ? 0
                       : focused ? root.width - (root.itemCount - 1) * (root.narrowWidth + root.gap)
                                 : root.narrowWidth

                Behavior on width {
                    NumberAnimation { duration: AppMotion.scaled(560); easing.type: Easing.OutBack; easing.overshoot: 0.9 }
                }

                Component.onCompleted: {
                    if (shown) {
                        Core.requestCatalogHeroCover(card.entryId)
                        Core.requestCatalogCover(card.entryId)
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: card.focused ? 32 : 24
                    color: MD.Token.color.surface_container_high
                    Behavior on radius { NumberAnimation { duration: AppMotion.medium } }
                }

                MD.Image {
                    anchors.fill: parent
                    radius: card.focused ? 32 : 24
                    source: card.focused ? card.banner : card.coverUrl
                    sourceSize: card.focused ? Qt.size(1280, 480) : Qt.size(240, 340)
                    asynchronous: true
                    scale: hover.hovered && !card.focused ? 1.0 : 1.0
                }

                // Legibility: a scrim on the left of the focused card, a dim on the waiting ones.
                Rectangle {
                    anchors.fill: parent
                    radius: card.focused ? 32 : 24
                    visible: card.focused
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.78) }
                        GradientStop { position: 0.55; color: Qt.rgba(0, 0, 0, 0.30) }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    radius: 24
                    visible: !card.focused
                    color: "black"
                    opacity: hover.hovered ? 0.1 : 0.35
                    Behavior on opacity { NumberAnimation { duration: AppMotion.short } }
                }

                // Copy of the focused card.
                ColumnLayout {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 32
                    width: Math.min(parent.width * 0.6, 560)
                    spacing: MD.Token.spacing.small
                    visible: card.focused
                    opacity: card.focused ? 1 : 0
                    Behavior on opacity {
                        SequentialAnimation {
                            PauseAnimation { duration: card.focused ? 180 : 0 }
                            NumberAnimation { duration: AppMotion.long; easing: AppMotion.emphasizedDecelerate }
                        }
                    }

                    MD.Label {
                        text: qsTr("Tonight's pick").toUpperCase()
                        typescale: MD.Token.typescale.label_large
                        font.letterSpacing: 1.6
                        color: MD.Token.color.primary
                    }

                    DisplayTitle {
                        Layout.fillWidth: true
                        size: card.title.length > 22 ? 32 : 40
                        color: "white"
                        text: card.title
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                    }

                    MD.Label {
                        visible: card.sizeLabel.length > 0
                        text: card.sizeLabel
                        typescale: MD.Token.typescale.title_medium
                        color: Qt.rgba(1, 1, 1, 0.8)
                    }

                    RowLayout {
                        Layout.topMargin: MD.Token.spacing.small
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            mdState.type: MD.Enum.BtFilled
                            mdState.size: MD.Enum.M
                            text: qsTr("Open game")
                            icon.name: MD.Token.icon.arrow_forward
                            onClicked: root.openGame(card.entryId)
                        }
                        MD.Button {
                            mdState.type: MD.Enum.BtFilledTonal
                            mdState.size: MD.Enum.M
                            text: qsTr("Next")
                            icon.name: MD.Token.icon.skip_next
                            visible: root.itemCount > 1
                            onClicked: root.focusIndex = (root.focusIndex + 1) % root.itemCount
                        }
                    }
                }

                // Title of a waiting card, written along its bottom.
                MD.Label {
                    visible: !card.focused
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 12
                    text: card.title
                    color: "white"
                    typescale: MD.Token.typescale.label_large
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                HoverHandler {
                    id: hover
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    onTapped: {
                        if (card.focused)
                            root.openGame(card.entryId)
                        else
                            root.focusIndex = card.index
                    }
                }
            }
        }
    }
}
