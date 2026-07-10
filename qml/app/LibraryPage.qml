import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int minCardWidth: 160
    readonly property int gridSpacing: MD.Token.spacing.medium
    readonly property int metaHeight: 48
    readonly property bool libraryEmpty: Core.library.count === 0
    readonly property int pageMargin: MD.Token.spacing.large
    readonly property int cardRadius: MD.Token.shape.corner.extra_large

    property string selectedSourceId: Core.sources.firstEnabledId
    property var featuredGame: Core.library.gameAt(0)

    signal openGame(string gameId)
    signal openCatalog()
    signal openSettings()
    signal addSourceRequested()

    Component.onCompleted: {
        if (selectedSourceId.length)
            Core.searchCatalog(selectedSourceId, "")
    }

    Connections {
        target: Core.library
        function onModelReset() { root.featuredGame = Core.library.gameAt(0) }
        function onDataChanged() { root.featuredGame = Core.library.gameAt(0) }
    }

    // ── Empty library ────────────────────────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: root.libraryEmpty

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: root.pageMargin
            anchors.rightMargin: root.pageMargin
            spacing: MD.Token.spacing.large

            Item {
                id: emptyHero
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                clip: true

                layer.enabled: true
                layer.effect: MD.RoundClip {
                    corners: MD.Util.corners(root.cardRadius)
                    size: Qt.vector2d(emptyHero.width, emptyHero.height)
                }

                Rectangle {
                    anchors.fill: parent
                    color: MD.Token.color.surface_container_high
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0.0
                            color: MD.Util.transparent(MD.Token.color.primary, 0.12)
                        }
                        GradientStop {
                            position: 1.0
                            color: "transparent"
                        }
                    }
                }

                SpiderWebMark {
                    width: 340
                    height: 340
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: -170
                    strokeColor: MD.Token.color.primary
                    strokeWidth: 3
                    opacity: 0.20
                }

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: MD.Token.spacing.extra_large
                    anchors.rightMargin: MD.Token.spacing.extra_large + 120
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Пока тут ничего нет")
                        typescale: MD.Token.typescale.headline_medium
                        wrapMode: Text.WordWrap
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 520
                        text: qsTr("Библиотека пуста. Добавьте источник каталога, установите игру — и она появится здесь.")
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.topMargin: MD.Token.spacing.small
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            text: Core.sources.enabledCount > 0
                                  ? qsTr("Открыть каталог")
                                  : qsTr("Добавить источник")
                            icon.name: Core.sources.enabledCount > 0
                                       ? MD.Token.icon.storefront
                                       : MD.Token.icon.add
                            mdState.type: MD.Enum.BtFilled
                            onClicked: {
                                if (Core.sources.enabledCount > 0)
                                    root.openCatalog()
                                else
                                    root.addSourceRequested()
                            }
                        }

                        MD.Button {
                            visible: Core.sources.enabledCount > 0
                            text: qsTr("Настройки")
                            icon.name: MD.Token.icon.settings
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: root.openSettings()
                        }

                        MD.Button {
                            visible: Core.sources.enabledCount === 0
                            text: qsTr("Что такое источник?")
                            mdState.type: MD.Enum.BtText
                            onClicked: root.openSettings()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: MD.Token.spacing.medium

                Repeater {
                    model: [
                        {
                            icon: MD.Token.icon.extension,
                            step: qsTr("Шаг 1"),
                            title: qsTr("Источник"),
                            body: qsTr("Укажите URL JSON-каталога (Hydra/FreeTP) в настройках.")
                        },
                        {
                            icon: MD.Token.icon.storefront,
                            step: qsTr("Шаг 2"),
                            title: qsTr("Каталог"),
                            body: qsTr("Выберите игру и запустите установку — торрент скачается сам.")
                        },
                        {
                            icon: MD.Token.icon.sports_esports,
                            step: qsTr("Шаг 3"),
                            title: qsTr("Библиотека"),
                            body: qsTr("Установленные игры живут здесь: запуск, обновления, детали.")
                        }
                    ]

                    Item {
                        id: tipHost
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.preferredHeight: tipInner.implicitHeight + MD.Token.spacing.large * 2

                        Item {
                            id: tipCard
                            anchors.fill: parent
                            clip: true

                            layer.enabled: true
                            layer.effect: MD.RoundClip {
                                corners: MD.Util.corners(root.cardRadius)
                                size: Qt.vector2d(tipCard.width, tipCard.height)
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: MD.Token.color.surface_container
                            }

                            ColumnLayout {
                                id: tipInner
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: MD.Token.spacing.large
                                spacing: MD.Token.spacing.small

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: MD.Token.spacing.medium

                                    Rectangle {
                                        Layout.preferredWidth: 48
                                        Layout.preferredHeight: 48
                                        radius: MD.Token.shape.corner.medium
                                        color: MD.Token.color.secondary_container

                                        MD.Icon {
                                            anchors.centerIn: parent
                                            name: tipHost.modelData.icon
                                            size: 24
                                            color: MD.Token.color.on_secondary_container
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: tipHost.modelData.step
                                            color: MD.Token.color.primary
                                            typescale: MD.Token.typescale.label_medium
                                        }

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: tipHost.modelData.title
                                            typescale: MD.Token.typescale.title_small
                                        }
                                    }
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: tipHost.modelData.body
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_medium
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Populated library ────────────────────────────────────────────────────
    Flickable {
        id: flick
        anchors.fill: parent
        visible: !root.libraryEmpty
        contentWidth: width
        contentHeight: contentCol.implicitHeight + MD.Token.spacing.large
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: flick.width
            spacing: MD.Token.spacing.large

            Item {
                id: heroHost
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                Layout.topMargin: MD.Token.spacing.medium
                Layout.preferredHeight: 280

                Item {
                    id: heroCard
                    anchors.fill: parent
                    clip: true

                    layer.enabled: true
                    layer.effect: MD.RoundClip {
                        corners: MD.Util.corners(root.cardRadius)
                        size: Qt.vector2d(heroCard.width, heroCard.height)
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: MD.Token.color.surface_container
                    }

                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop {
                                position: 0.0
                                color: MD.Util.transparent(MD.Token.color.primary, 0.10)
                            }
                            GradientStop {
                                position: 1.0
                                color: "transparent"
                            }
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: MD.Token.spacing.large
                        spacing: MD.Token.spacing.large

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: MD.Token.spacing.small

                            MD.Label {
                                Layout.fillWidth: true
                                text: qsTr("Недавно играли")
                                color: MD.Token.color.primary
                                typescale: MD.Token.typescale.label_large
                                elide: Text.ElideRight
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: root.featuredGame.title ?? ""
                                typescale: MD.Token.typescale.headline_large
                                elide: Text.ElideRight
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: (root.featuredGame.sourceName ?? "") + " · v" + (root.featuredGame.version ?? "")
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_large
                                elide: Text.ElideRight
                            }

                            Item { Layout.fillHeight: true }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: MD.Token.spacing.small

                                MD.Button {
                                    text: qsTr("Играть")
                                    mdState.type: MD.Enum.BtFilled
                                    enabled: !!(root.featuredGame.gameId)
                                    onClicked: Core.launchGame(root.featuredGame.gameId)
                                }

                                MD.Button {
                                    text: qsTr("Подробнее")
                                    mdState.type: MD.Enum.BtOutlined
                                    enabled: !!(root.featuredGame.gameId)
                                    onClicked: root.openGame(root.featuredGame.gameId)
                                }

                                MD.Button {
                                    text: qsTr("Обновления")
                                    mdState.type: MD.Enum.BtText
                                    onClicked: Core.checkUpdates()
                                }
                            }
                        }

                        GamePoster {
                            Layout.preferredWidth: 140
                            Layout.preferredHeight: 186
                            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                            source: root.featuredGame.coverUrl ?? ""
                            seed: root.featuredGame.title ?? ""
                            fallbackText: (root.featuredGame.title ?? "?").charAt(0)
                            cornerRadius: root.cardRadius
                            onClicked: {
                                if (root.featuredGame.gameId)
                                    root.openGame(root.featuredGame.gameId)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                spacing: MD.Token.spacing.medium

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("В библиотеке")
                    value: String(Core.library.count)
                    iconName: MD.Token.icon.sports_esports
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Источники")
                    value: String(Core.sources.enabledCount)
                    iconName: MD.Token.icon.storefront
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Задачи")
                    value: String(Core.jobs.count)
                    iconName: MD.Token.icon.downloading
                }

                StatCard {
                    Layout.fillWidth: true
                    title: qsTr("Обновления")
                    value: String(Core.library.updateCount())
                    iconName: MD.Token.icon.update
                }
            }

            MD.Pane {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin
                visible: Core.jobs.count > 0
                padding: MD.Token.spacing.medium
                radius: root.cardRadius
                backgroundColor: MD.Token.color.secondary_container
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Активные задачи")
                        color: MD.Token.color.on_secondary_container
                        typescale: MD.Token.typescale.title_small
                        elide: Text.ElideRight
                    }

                    Repeater {
                        model: Core.jobs

                        DownloadJobCard {
                            required property string jobId
                            required property string title
                            required property string kindLabel
                            required property string status
                            required property int progress
                            required property string detail
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Моя библиотека")
                    typescale: MD.Token.typescale.title_large
                }

                MD.Label {
                    text: qsTr("%1 игр").arg(Core.library.count)
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.label_large
                }
            }

            Item {
                id: gridHost
                Layout.fillWidth: true
                Layout.leftMargin: root.pageMargin
                Layout.rightMargin: root.pageMargin

                property int columns: 3
                property real cardWidth: 160
                property real cardHeight: 260

                function relayout() {
                    const cols = Math.max(2, Math.floor((width + root.gridSpacing) / (root.minCardWidth + root.gridSpacing)))
                    const cardW = (width - root.gridSpacing * (cols - 1)) / cols
                    columns = cols
                    cardWidth = cardW
                    cardHeight = cardW * 4 / 3 + root.metaHeight
                    const rows = Math.max(1, Math.ceil(Core.library.count / cols))
                    Layout.preferredHeight = rows * (cardHeight + root.gridSpacing)
                }

                Timer {
                    id: layoutTimer
                    interval: 50
                    onTriggered: gridHost.relayout()
                }

                onWidthChanged: layoutTimer.restart()
                Component.onCompleted: relayout()

                Connections {
                    target: Core.library
                    function onModelReset() { gridHost.relayout() }
                    function onRowsInserted() { gridHost.relayout() }
                    function onRowsRemoved() { gridHost.relayout() }
                }

                GridView {
                    anchors.fill: parent
                    clip: true
                    interactive: false
                    model: Core.library
                    cellWidth: gridHost.cardWidth + root.gridSpacing
                    cellHeight: gridHost.cardHeight + root.gridSpacing
                    cacheBuffer: 0
                    delegate: LibraryGameCard {
                        width: Math.max(0, gridHost.cardWidth - root.gridSpacing)
                        height: gridHost.cardHeight
                        onOpenDetails: function (id) { root.openGame(id) }
                    }
                }
            }
        }
    }
}
