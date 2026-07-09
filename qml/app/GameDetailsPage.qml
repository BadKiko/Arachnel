import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    property string gameId: ""
    property bool fromCatalog: false

    readonly property var info: {
        if (!gameId.length)
            return ({})
        if (fromCatalog)
            return Core.catalog.entryInfo(gameId)
        return Core.library.gameInfo(gameId)
    }

    readonly property bool installed: !!(info.installed)
    readonly property string sourceLabel: {
        const sid = info.sourceId ?? ""
        if (!sid.length)
            return ""
        if (info.sourceName && info.sourceName.length && info.sourceName !== sid)
            return info.sourceName
        return Core.sources.nameForId(sid)
    }

    signal backRequested()

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentCol.implicitHeight + MD.Token.spacing.large
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: contentCol
            width: flick.width
            spacing: MD.Token.spacing.large

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.topMargin: MD.Token.spacing.medium
                spacing: MD.Token.spacing.small

                MD.IconButton {
                    mdState.type: MD.Enum.IBtStandard
                    icon.name: MD.Token.icon.arrow_back
                    onClicked: root.backRequested()
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Детали игры")
                    typescale: MD.Token.typescale.title_large
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                spacing: MD.Token.spacing.large

                GamePoster {
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 293
                    Layout.alignment: Qt.AlignTop
                    source: root.info.coverUrl ?? ""
                    fallbackText: (root.info.title ?? "?").charAt(0)
                    cornerRadius: MD.Token.shape.corner.extra_large
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.info.title ?? qsTr("Игра не найдена")
                        typescale: MD.Token.typescale.headline_medium
                        wrapMode: Text.WordWrap
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: !!(root.info.genres)
                        text: root.info.genres ?? ""
                        color: MD.Token.color.primary
                        typescale: MD.Token.typescale.label_large
                        wrapMode: Text.WordWrap
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: MD.Token.spacing.small

                        MD.AssistChip {
                            text: root.sourceLabel
                            icon.name: MD.Token.icon.storefront
                        }
                        MD.AssistChip {
                            text: "v" + (root.info.version ?? "")
                            icon.name: MD.Token.icon.tag
                        }
                        MD.AssistChip {
                            visible: !!(root.info.sizeLabel)
                            text: root.info.sizeLabel ?? ""
                            icon.name: MD.Token.icon.hard_drive
                        }
                        MD.AssistChip {
                            text: root.info.installKindLabel ?? ""
                            icon.name: MD.Token.icon.install_desktop
                        }
                        MD.AssistChip {
                            visible: !!(root.info.hasUpdate)
                            text: qsTr("Есть обновление")
                            icon.name: MD.Token.icon.update
                        }
                    }

                    RowLayout {
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            visible: root.installed
                            text: qsTr("Играть")
                            icon.name: MD.Token.icon.play_arrow
                            mdState.type: MD.Enum.BtFilled
                            onClicked: Core.launchGame(root.gameId)
                        }

                        MD.Button {
                            visible: !root.installed
                            text: qsTr("Установить")
                            icon.name: MD.Token.icon.download
                            mdState.type: MD.Enum.BtFilled
                            onClicked: Core.installCatalogEntry(root.gameId)
                        }

                        MD.Button {
                            visible: root.installed && !!(root.info.hasUpdate)
                            text: qsTr("Обновить")
                            icon.name: MD.Token.icon.update
                            mdState.type: MD.Enum.BtFilledTonal
                            onClicked: Core.checkUpdates()
                        }
                    }
                }
            }

            MD.ElevationRectangle {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.preferredHeight: aboutCol.implicitHeight + 2 * MD.Token.spacing.large
                radius: MD.Token.shape.corner.extra_large
                color: MD.Token.color.surface_container
                elevation: MD.Token.elevation.level0

                ColumnLayout {
                    id: aboutCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.large
                    spacing: MD.Token.spacing.medium

                    MD.Label {
                        text: qsTr("Описание")
                        typescale: MD.Token.typescale.title_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: root.info.description || qsTr("Описание пока недоступно.")
                        typescale: MD.Token.typescale.body_large
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                    }
                }
            }

            MD.ElevationRectangle {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.preferredHeight: metaCol.implicitHeight + 2 * MD.Token.spacing.large
                radius: MD.Token.shape.corner.extra_large
                color: MD.Token.color.surface_container
                elevation: MD.Token.elevation.level0

                ColumnLayout {
                    id: metaCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.large
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        text: qsTr("Информация")
                        typescale: MD.Token.typescale.title_medium
                    }

                    Repeater {
                        model: [
                            { label: qsTr("Источник"), value: root.sourceLabel },
                            { label: qsTr("Версия"), value: root.info.version ?? "" },
                            { label: qsTr("Размер"), value: root.info.sizeLabel || "—" },
                            { label: qsTr("Тип установки"), value: root.info.installKindLabel ?? "" },
                            {
                                label: qsTr("Путь"),
                                value: root.info.installPath || qsTr("Не установлена")
                            }
                        ]

                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.medium

                            MD.Label {
                                Layout.preferredWidth: 140
                                text: modelData.label
                                color: MD.Token.color.on_surface_variant
                                typescale: MD.Token.typescale.body_medium
                            }

                            MD.Label {
                                Layout.fillWidth: true
                                text: modelData.value
                                typescale: MD.Token.typescale.body_medium
                                elide: Text.ElideMiddle
                            }
                        }
                    }
                }
            }
        }
    }
}
