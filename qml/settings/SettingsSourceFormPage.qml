import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property bool editing: false
    property string sourceId: ""
    property bool sourceEnabled: true
    property bool openCreate: false

    signal saved()
    signal cancelled()

    anchors.fill: parent

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    function loadCreate() {
        editing = false
        sourceId = ""
        sourceEnabled = true
        nameField.text = ""
        urlField.text = ""
        descriptionField.text = ""
        errorLabel.text = ""
        nameField.forceActiveFocus()
    }

    function loadEdit(id, name, catalogUrl, description, enabled) {
        editing = true
        sourceId = id
        sourceEnabled = !!enabled
        nameField.text = name
        urlField.text = catalogUrl
        descriptionField.text = description
        errorLabel.text = ""
        nameField.forceActiveFocus()
    }

    Component.onCompleted: {
        if (openCreate)
            loadCreate()
    }

    function save() {
        const name = nameField.text.trim()
        const url = urlField.text.trim()
        const description = descriptionField.text.trim()

        if (!name.length || !url.length) {
            errorLabel.text = qsTr("Укажите название и URL каталога (JSON).")
            return false
        }

        if (editing) {
            const ok = Core.sources.updateSource(sourceId, name, url, description, "",
                                                 root.sourceEnabled)
            if (!ok) {
                errorLabel.text = qsTr("Не удалось сохранить изменения.")
                return false
            }
        } else {
            const id = Core.sources.addSource(name, url, description, "")
            if (!id.length) {
                errorLabel.text = qsTr("Не удалось добавить источник.")
                return false
            }
        }

        errorLabel.text = ""
        root.saved()
        return true
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
            text: qsTr("Источник — JSON-каталог в формате Hydra/FreeTP. Arachnel загрузит список игр по URL.")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        MD.TextField {
            id: nameField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            placeholderText: qsTr("Название")
            onTextChanged: errorLabel.text = ""
        }

        MD.TextField {
            id: urlField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            placeholderText: qsTr("URL каталога (JSON)")
            onTextChanged: errorLabel.text = ""
        }

        MD.TextField {
            id: descriptionField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            placeholderText: qsTr("Краткое описание (необязательно)")
        }

        MD.Label {
            id: errorLabel
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            visible: text.length > 0
            color: MD.Token.color.error
            typescale: MD.Token.typescale.body_small
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            Layout.bottomMargin: MD.Token.spacing.medium
            spacing: MD.Token.spacing.small

            Item { Layout.fillWidth: true }

            MD.Button {
                mdState.type: MD.Enum.BtText
                text: qsTr("Отмена")
                onClicked: root.cancelled()
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilled
                text: root.editing ? qsTr("Сохранить") : qsTr("Добавить")
                onClicked: root.save()
            }
        }
    }
}
