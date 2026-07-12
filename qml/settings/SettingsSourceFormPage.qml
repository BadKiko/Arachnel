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
    property bool validating: false
    property string validateRequestId: ""
    property string pendingUrl: ""

    signal saved()
    signal cancelled()

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
            errorLabel.text = qsTr("Enter a name and catalog URL.")
            return false
        }

        if (root.validating)
            return false

        root.validating = true
        root.pendingUrl = url
        root.validateRequestId = Date.now().toString()
        errorLabel.text = qsTr("Validating catalog…")
        Core.validateHydraCatalogUrl(root.validateRequestId, url)
        return false
    }

    function commitSave(validatedCount) {
        const name = nameField.text.trim()
        const url = root.pendingUrl.length ? root.pendingUrl : urlField.text.trim()
        const description = descriptionField.text.trim()

        if (root.editing) {
            Core.invalidateSourceCatalog(root.sourceId)
            const ok = Core.sources.updateSource(root.sourceId, name, url, description, "",
                                                 root.sourceEnabled)
            if (!ok) {
                errorLabel.text = qsTr("Could not save changes.")
                return false
            }
        } else {
            const id = Core.sources.addSource(name, url, description, "")
            if (!id.length) {
                errorLabel.text = qsTr("Could not add catalog.")
                return false
            }
        }

        errorLabel.text = ""
        Core.prefetchCatalogCounts()
        root.saved()
        return true
    }

    Connections {
        target: Core
        function onHydraCatalogUrlValidated(requestId, ok, count, error) {
            if (requestId !== root.validateRequestId)
                return

            root.validating = false
            if (!ok) {
                errorLabel.text = error.length
                              ? error
                              : qsTr("Could not load catalog from this URL.")
                return
            }

            errorLabel.text = qsTr("Games found: %1").arg(count)
            root.commitSave(count)
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
            text: qsTr("Hydra catalog — a games.json JSON feed by URL. Arachnel pulls the game list and magnet links like Hydra Launcher. Install and launch via a source plugin (e.g. FreeTP).")
            wrapMode: Text.WordWrap
            color: MD.Token.color.on_surface_variant
            typescale: MD.Token.typescale.body_medium
        }

        MD.TextField {
            id: nameField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            placeholderText: qsTr("Name")
            onTextChanged: errorLabel.text = ""
        }

        MD.TextField {
            id: urlField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            placeholderText: qsTr("URL games.json")
            onTextChanged: errorLabel.text = ""
        }

        MD.TextField {
            id: descriptionField
            Layout.fillWidth: true
            Layout.leftMargin: contentMargin
            Layout.rightMargin: contentMargin
            placeholderText: qsTr("Short description (optional)")
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
                text: qsTr("Cancel")
                onClicked: root.cancelled()
            }

            MD.Button {
                mdState.type: MD.Enum.BtFilled
                text: root.validating
                      ? qsTr("Validating…")
                      : (root.editing ? qsTr("Save") : qsTr("Add"))
                enabled: !root.validating
                onClicked: root.save()
            }
        }
    }
}
