import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property int countsRevision: 0

    signal addSourceRequested()
    signal editSourceRequested(string pluginId, string name, string catalogUrl,
                               string description, bool sourceEnabled)

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    function formatGameCount(sourceId) {
        root.countsRevision
        const count = Core.catalogEntryCount(sourceId)
        if (count < 0)
            return qsTr("Игр: …")
        return qsTr("Игр: %1").arg(count)
    }

    Component.onCompleted: Core.prefetchCatalogCounts()

    Connections {
        target: Core
        function onCatalogCountsChanged() {
            root.countsRevision++
        }
    }

    Connections {
        target: Core.sources
        function onSourcesChanged() {
            Core.prefetchCatalogCounts()
        }
    }

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Подключите каталоги в формате Hydra Launcher (games.json). Удобно при переходе с Hydra — те же ссылки на игры, загрузка через торрент. Для установки и запуска нужен плагин источника.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: Core.sources.count === 0
            radius: MD.Token.shape.corner.large
            color: MD.Token.color.surface_container
            border.width: 1
            border.color: MD.Token.color.outline_variant
            implicitHeight: emptySourcesCol.implicitHeight + MD.Token.spacing.medium * 2

            ColumnLayout {
                id: emptySourcesCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: MD.Token.spacing.medium
                spacing: MD.Token.spacing.extra_small

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Пока нет каталогов")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Нажмите «Добавить каталог» и вставьте URL вашего games.json — как в Hydra, или публичный фид сообщества.")
                    wrapMode: Text.WordWrap
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            spacing: MD.Token.spacing.small
            visible: Core.sources.count > 0

            Repeater {
                model: Core.sources

                Rectangle {
                    id: sourceCard
                    required property string pluginId
                    required property string name
                    required property string description
                    required property string catalogUrl
                    required property bool sourceEnabled
                    required property bool hasCatalogUrl
                    required property bool isPlugin
                    required property string pluginVersion

                    Layout.fillWidth: true
                    radius: MD.Token.shape.corner.large
                    color: MD.Token.color.surface_container_low
                    border.width: 1
                    border.color: MD.Token.color.outline_variant
                    implicitHeight: sourceCardBody.implicitHeight + MD.Token.spacing.medium * 2

                    ColumnLayout {
                        id: sourceCardBody
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: MD.Token.spacing.medium
                        spacing: MD.Token.spacing.small

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.medium

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: sourceCard.name
                                    typescale: MD.Token.typescale.title_small
                                    elide: Text.ElideRight
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: sourceCard.description.length
                                          ? sourceCard.description
                                          : sourceCard.catalogUrl
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_small
                                    elide: Text.ElideMiddle
                                    maximumLineCount: 1
                                }
                            }

                            MD.Switch {
                                checked: sourceCard.sourceEnabled
                                onToggled: Core.sources.setSourceEnabled(sourceCard.pluginId, checked)
                            }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: sourceCard.isPlugin && sourceCard.pluginVersion.length > 0
                            text: qsTr("Плагин · v%1").arg(sourceCard.pluginVersion)
                            color: MD.Token.color.primary
                            typescale: MD.Token.typescale.label_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: !sourceCard.isPlugin && sourceCard.catalogUrl.length > 0
                            text: sourceCard.catalogUrl
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.label_small
                            elide: Text.ElideMiddle
                            maximumLineCount: 1
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: sourceCard.hasCatalogUrl
                            text: root.formatGameCount(sourceCard.pluginId)
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.label_medium
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small

                            MD.Label {
                                Layout.fillWidth: true
                                text: !sourceCard.hasCatalogUrl
                                      ? qsTr("Нет URL — каталог не загрузится")
                                      : (sourceCard.sourceEnabled
                                         ? qsTr("Активен в каталоге")
                                         : qsTr("Выключен"))
                                color: sourceCard.hasCatalogUrl
                                       ? MD.Token.color.on_surface_variant
                                       : MD.Token.color.error
                                typescale: MD.Token.typescale.label_medium
                            }

                            MD.Button {
                                visible: !sourceCard.isPlugin
                                mdState.type: MD.Enum.BtText
                                text: qsTr("Изменить")
                                onClicked: root.editSourceRequested(
                                               sourceCard.pluginId,
                                               sourceCard.name,
                                               sourceCard.catalogUrl,
                                               sourceCard.description,
                                               sourceCard.sourceEnabled)
                            }

                            MD.Button {
                                visible: !sourceCard.isPlugin
                                mdState.type: MD.Enum.BtText
                                text: qsTr("Удалить")
                                onClicked: Core.sources.removeSource(sourceCard.pluginId)
                            }
                        }
                    }
                }
            }
        }

        MD.Button {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            mdState.type: MD.Enum.BtFilledTonal
            text: qsTr("Добавить каталог Hydra")
            onClicked: root.addSourceRequested()
        }
    }
}
