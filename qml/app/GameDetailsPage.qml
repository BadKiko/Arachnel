import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    property string gameId: ""
    property bool fromCatalog: false

    // Opaque surface so catalog doesn't ghost through during page fade/bounce.
    Rectangle {
        anchors.fill: parent
        color: MD.Token.color.surface
    }

    readonly property var info: {
        if (!gameId.length)
            return ({})
        if (fromCatalog)
            return Core.catalog.entryInfo(gameId)
        return Core.library.gameInfo(gameId)
    }

    readonly property bool installed: !!(info.installed)

    property var downloadJob: ({})

    readonly property bool downloadPaused: downloadJob.status === "paused" || !!downloadJob.paused
    readonly property bool downloadActive: !!(downloadJob.inProgress) && !downloadPaused
    readonly property bool downloadCompleted: downloadJob.status === "completed"
    readonly property bool showDownloadProgress: !!(downloadJob.inProgress) || downloadCompleted

    function refreshDownloadJob() {
        downloadJob = Core.jobs.jobForEntry(root.gameId)
    }

    Connections {
        target: Core.jobs
        function onJobsChanged() { root.refreshDownloadJob() }
    }

    onGameIdChanged: {
        refreshDownloadJob()
        maybeEnrich()
    }
    onFromCatalogChanged: maybeEnrich()

    function maybeEnrich() {
        if (gameId.length > 0 && fromCatalog)
            Core.enrichCatalogEntry(gameId)
    }

    Component.onCompleted: {
        refreshDownloadJob()
        maybeEnrich()
    }

    readonly property string sourceLabel: {
        const sid = info.sourceId ?? ""
        if (!sid.length)
            return ""
        if (info.sourceName && info.sourceName.length && info.sourceName !== sid)
            return info.sourceName
        return Core.sources.nameForId(sid)
    }

    signal backRequested()
    signal openInstallPicker(string entryId, string title)

    function beginInstall() {
        if (Core.needsInstallLocationChoice())
            root.openInstallPicker(root.gameId, root.info.title || "")
        else
            Core.installCatalogEntry(root.gameId)
    }

    function confirmRemove() {
        Core.removeGame(root.gameId, true)
        root.backRequested()
    }

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
                            visible: !!(root.info.hasAddons)
                            text: qsTr("%1 доп.").arg(root.info.addonCount ?? 0)
                            icon.name: MD.Token.icon.extension
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

                    MD.Label {
                        Layout.fillWidth: true
                        visible: root.installed && !(root.info.installPath && root.info.installPath.length)
                        text: qsTr("Торрент загружен. Установка будет выполнена плагином источника.")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    RowLayout {
                        spacing: MD.Token.spacing.small

                        MD.Button {
                            visible: root.installed && !!(root.info.installPath && root.info.installPath.length)
                            text: qsTr("Играть")
                            icon.name: MD.Token.icon.play_arrow
                            mdState.type: MD.Enum.BtFilled
                            onClicked: Core.launchGame(root.gameId)
                        }

                        DownloadProgressButton {
                            visible: (root.fromCatalog || !root.installed || root.showDownloadProgress)
                                     && !(root.installed && !!(root.info.installPath && root.info.installPath.length))
                            progress: root.downloadJob.progress ?? 0
                            downloading: root.downloadActive
                            paused: root.downloadPaused
                            completed: root.downloadCompleted
                            onActivated: root.beginInstall()
                            onPauseToggleRequested: Core.toggleJobPause(root.downloadJob.jobId)
                        }

                        MD.Button {
                            visible: root.installed
                            text: qsTr("Удалить")
                            icon.name: MD.Token.icon.delete
                            mdState.type: MD.Enum.BtOutlined
                            onClicked: removeDialog.open()
                        }

                        MD.Button {
                            visible: root.installed && !!(root.info.hasUpdate) && !root.downloadJob.inProgress
                            text: qsTr("Обновить")
                            icon.name: MD.Token.icon.update
                            mdState.type: MD.Enum.BtFilledTonal
                            onClicked: Core.updateCatalogEntry(root.gameId)
                        }
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        visible: root.showDownloadProgress && !root.downloadCompleted && !!(root.downloadJob.detail)
                        text: root.downloadJob.detail ?? ""
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_medium
                        elide: Text.ElideRight
                        maximumLineCount: 1
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
                visible: (root.info.addonCount ?? 0) > 0
                Layout.preferredHeight: addonsCol.implicitHeight + 2 * MD.Token.spacing.large
                radius: MD.Token.shape.corner.extra_large
                color: MD.Token.color.surface_container
                elevation: MD.Token.elevation.level0

                ColumnLayout {
                    id: addonsCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: MD.Token.spacing.large
                    spacing: MD.Token.spacing.small

                    MD.Label {
                        text: qsTr("Дополнения")
                        typescale: MD.Token.typescale.title_medium
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("У FreeTP к игре могут идти отдельные DLC — их нужно докачать и установить отдельно.")
                        wrapMode: Text.WordWrap
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.body_medium
                    }

                    Repeater {
                        model: Core.catalog.addonsFor(root.gameId)

                        RowLayout {
                            required property string id
                            required property string title
                            required property string fileSize
                            required property string kindLabel
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                MD.Label {
                                    Layout.fillWidth: true
                                    text: title
                                    typescale: MD.Token.typescale.body_large
                                    elide: Text.ElideRight
                                }
                                MD.Label {
                                    text: kindLabel + " · " + fileSize
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.label_medium
                                }
                            }

                            MD.Button {
                                text: qsTr("Скачать")
                                icon.name: MD.Token.icon.download
                                mdState.type: MD.Enum.BtOutlined
                                onClicked: Core.installCatalogAddon(root.gameId, id)
                            }
                        }
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
                                label: qsTr("Путь установки"),
                                value: root.info.installPath || qsTr("Ожидает установки плагином")
                            },
                            {
                                label: qsTr("Загрузка"),
                                value: root.info.downloadPath || "—"
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

    MD.Dialog {
        id: removeDialog
        title: qsTr("Удалить игру?")

        MD.Label {
            text: qsTr("Файлы игры будут удалены с диска. Это действие нельзя отменить.")
            wrapMode: Text.WordWrap
            typescale: MD.Token.typescale.body_medium
        }

        MD.DialogButtonBox {
            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Отмена")
                onClicked: removeDialog.close()
            }
            MD.Button {
                mdState.type: MD.Enum.BtFilled
                text: qsTr("Удалить")
                onClicked: {
                    removeDialog.close()
                    root.confirmRemove()
                }
            }
        }
    }
}
