import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property bool applying: false

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    function syncFromStore() {
        applying = true
        libraryPathField.text = Core.settings.libraryRoot
        downloadsPathField.text = Core.settings.downloadsRoot
        applying = false
    }

    Component.onCompleted: syncFromStore()

    ColumnLayout {
        id: body
        width: root.width
        spacing: MD.Token.spacing.medium

        MD.Label {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.topMargin: MD.Token.spacing.small
            text: qsTr("Папка библиотеки — куда устанавливаются игры. Папка загрузок — торренты до установки.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        MD.TextField {
            id: libraryPathField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            placeholderText: qsTr("Папка библиотеки")
            onEditingFinished: {
                if (!root.applying)
                    Core.settings.libraryRoot = text
            }
        }

        MD.TextField {
            id: downloadsPathField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            placeholderText: qsTr("Папка загрузок")
            onEditingFinished: {
                if (!root.applying)
                    Core.settings.downloadsRoot = text
            }
        }
    }
}
