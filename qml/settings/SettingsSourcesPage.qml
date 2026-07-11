import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large

    signal addSourceRequested()
    signal editSourceRequested(string pluginId, string name, string catalogUrl,
                               string description, bool sourceEnabled)

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Добавьте JSON-каталоги и включайте нужные — они появятся чипами в разделе «Каталог».")
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
                    text: qsTr("Пока нет источников")
                    typescale: MD.Token.typescale.title_small
                }

                MD.Label {
                    Layout.fillWidth: true
                    text: qsTr("Нажмите «Добавить источник» и вставьте URL каталога — например FreeTP или свой JSON.")
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
            text: qsTr("Добавить JSON-источник")
            onClicked: root.addSourceRequested()
        }
    }
}
