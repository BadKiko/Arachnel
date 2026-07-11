import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int pageMargin: MD.Token.spacing.large
    readonly property bool downloadsEmpty: Core.jobs.count === 0

    function countFinished() {
        return Core.jobs.count - Core.jobs.activeCount
    }

    // ── Empty (как «Нет игр» в каталоге) ─────────────────────────────────────
    Item {
        anchors.fill: parent
        visible: root.downloadsEmpty

        ColumnLayout {
            anchors.centerIn: parent
            spacing: MD.Token.spacing.medium
            width: Math.min(parent.width - pageMargin * 2, 420)

            SpiderWebMark {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 160
                Layout.preferredHeight: 160
                width: 160
                height: 160
                strokeColor: MD.Token.color.primary
                strokeWidth: 2.5
                opacity: 0.35
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Нет загрузок")
                typescale: MD.Token.typescale.title_large
            }

            MD.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Начните установку из каталога — прогресс появится здесь.")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_medium
                wrapMode: Text.WordWrap
            }
        }
    }

    // ── Список загрузок ────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: MD.Token.spacing.medium
        visible: !root.downloadsEmpty

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            Layout.topMargin: MD.Token.spacing.medium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                MD.Label {
                    text: qsTr("Загрузки")
                    typescale: MD.Token.typescale.headline_medium
                }

                MD.Label {
                    text: Core.jobs.activeCount > 0
                          ? qsTr("%1 активных · докачка после перезапуска").arg(Core.jobs.activeCount)
                          : qsTr("Торренты докачиваются после перезапуска")
                    color: MD.Token.color.on_surface_variant
                    typescale: MD.Token.typescale.body_medium
                }
            }

            MD.IconButton {
                visible: root.countFinished() > 0
                mdState.type: MD.Enum.IBtStandard
                icon.name: MD.Token.icon.delete_sweep
                onClicked: Core.clearFinishedJobs()
            }
        }

        ListView {
            id: jobsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: pageMargin
            Layout.rightMargin: pageMargin
            Layout.bottomMargin: pageMargin
            clip: true
            spacing: MD.Token.spacing.small
            boundsBehavior: Flickable.StopAtBounds
            model: Core.jobs

            ScrollBar.vertical: MD.ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: DownloadJobCard {
                width: jobsList.width
                jobId: model.jobId
                title: model.title
                kindLabel: model.kindLabel
                status: model.status
                statusLabel: model.statusLabel
                progress: model.progress
                detail: model.detail
                coverUrl: model.coverUrl
                entryId: model.entryId
            }
        }
    }
}
