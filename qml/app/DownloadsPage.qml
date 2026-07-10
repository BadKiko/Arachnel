import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    readonly property int pageMargin: MD.Token.spacing.large

    function countFinished() {
        return Core.jobs.count - Core.jobs.activeCount
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: MD.Token.spacing.medium

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

            MD.Button {
                text: qsTr("Очистить")
                mdState.type: MD.Enum.BtText
                enabled: root.countFinished() > 0
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

            MD.Label {
                anchors.centerIn: parent
                visible: jobsList.count === 0
                text: qsTr("Нет загрузок")
                color: MD.Token.color.on_surface_variant
                typescale: MD.Token.typescale.body_large
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                width: Math.min(jobsList.width, 360)
            }
        }
    }
}
