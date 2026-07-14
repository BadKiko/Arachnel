import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var closeSheet

    spacing: 0

    readonly property int contentMargin: MD.Token.spacing.large
    readonly property int pageHeight: 560

    property string pendingSection: ""
    property bool pendingCreateSource: false

    readonly property bool onLinux: Qt.platform.os === "linux"

    readonly property string pageTitle: {
        const item = stack.currentItem
        if (!item)
            return qsTr("Settings")
        if (item.pageTitle)
            return item.pageTitle
        return qsTr("Settings")
    }

    function syncFromStore() {
        Appearance.apply()
        rebuildHub()

        // While the sheet is sliding in, push targets without a second fade —
        // otherwise sheet enter + page fade stack and feel jumpy.
        if (pendingSection === "sources") {
            pendingSection = ""
            if (pendingCreateSource) {
                pendingCreateSource = false
                stack.navigatePush(sourceFormComponent, { openCreate: true }, true)
            } else {
                stack.navigatePush(sourcesComponent, {}, true)
            }
            stack.restoreCurrent()
        } else if (pendingSection.length) {
            const section = pendingSection
            pendingSection = ""
            if (section === "storage")
                stack.navigatePush(storageComponent, {}, true)
            else if (section === "updates")
                stack.navigatePush(updatesComponent, {}, true)
            else if (section === "launch" && root.onLinux)
                stack.navigatePush(launchComponent, {}, true)
            else if (section === "appearance")
                stack.navigatePush(appearanceComponent, {}, true)
            else if (section === "plugins")
                stack.navigatePush(pluginsComponent, {}, true)
            else
                openSection(section)
        } else {
            pendingCreateSource = false
        }
    }

    function prepareOpen(sectionId, createSource) {
        pendingSection = sectionId || ""
        pendingCreateSource = !!createSource
    }

    function rebuildHub() {
        stack.navigateReset(hubComponent)
    }

    function openSection(sectionId) {
        if (sectionId === "plugins")
            stack.navigatePush(pluginsComponent)
        else if (sectionId === "sources")
            openSources()
        else if (sectionId === "storage")
            stack.navigatePush(storageComponent)
        else if (sectionId === "updates")
            stack.navigatePush(updatesComponent)
        else if (sectionId === "launch" && root.onLinux)
            stack.navigatePush(launchComponent)
        else if (sectionId === "appearance")
            stack.navigatePush(appearanceComponent)
    }

    function openSources() {
        stack.navigatePush(sourcesComponent)
    }

    function openSourceCreate() {
        const page = stack.navigatePush(sourceFormComponent)
        if (page)
            page.loadCreate()
    }

    function openSourceEdit(pluginId, name, catalogUrl, description, sourceEnabled) {
        const page = stack.navigatePush(sourceFormComponent)
        if (page)
            page.loadEdit(pluginId, name, catalogUrl, description, sourceEnabled)
    }

    function goBack() {
        if (stack.canPop) {
            stack.navigatePop()
            return
        }
        root.closeSheet()
    }

    function resetOnClose() {
        pendingSection = ""
        pendingCreateSource = false
        rebuildHub()
    }

    Component {
        id: hubComponent
        SettingsHubPage {
            property string pageTitle: qsTr("Settings")
            contentMargin: root.contentMargin
            onOpenSection: function (sectionId) { root.openSection(sectionId) }
        }
    }

    Component {
        id: pluginsComponent
        SettingsPluginsPage {
            property string pageTitle: qsTr("Plugins")
            contentMargin: root.contentMargin
        }
    }

    Component {
        id: sourcesComponent
        SettingsSourcesPage {
            property string pageTitle: qsTr("Hydra catalogs")
            contentMargin: root.contentMargin
            onAddSourceRequested: root.openSourceCreate()
            onEditSourceRequested: function (pluginId, name, catalogUrl, description, sourceEnabled) {
                root.openSourceEdit(pluginId, name, catalogUrl, description, sourceEnabled)
            }
        }
    }

    Component {
        id: sourceFormComponent
        SettingsSourceFormPage {
            property string pageTitle: editing ? qsTr("Edit catalog") : qsTr("New Hydra catalog")
            contentMargin: root.contentMargin
            onSaved: stack.navigatePop()
            onCancelled: stack.navigatePop()
        }
    }

    Component {
        id: storageComponent
        SettingsStoragePage {
            property string pageTitle: qsTr("Storage")
            contentMargin: root.contentMargin
        }
    }

    Component {
        id: updatesComponent
        SettingsUpdatesPage {
            property string pageTitle: qsTr("Updates")
            contentMargin: root.contentMargin
        }
    }

    Component {
        id: launchComponent
        SettingsLaunchPage {
            property string pageTitle: qsTr("Launch")
            contentMargin: root.contentMargin
        }
    }

    Component {
        id: appearanceComponent
        SettingsAppearancePage {
            property string pageTitle: qsTr("Appearance")
            contentMargin: root.contentMargin
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: MD.Token.spacing.small
        Layout.rightMargin: contentMargin
        Layout.topMargin: MD.Token.spacing.small
        spacing: MD.Token.spacing.extra_small

        MD.IconButton {
            mdState.type: MD.Enum.IBtStandard
            icon.name: stack.depth > 1 ? MD.Token.icon.arrow_back : MD.Token.icon.close
            onClicked: root.goBack()
        }

        MD.Label {
            Layout.fillWidth: true
            text: root.pageTitle
            typescale: MD.Token.typescale.headline_small
            elide: Text.ElideRight
        }
    }

    PageNavigator {
        id: stack
        Layout.fillWidth: true
        Layout.preferredHeight: root.pageHeight
        clip: true
        Component.onCompleted: rebuildHub()
    }

    MD.Divider {
        Layout.fillWidth: true
        Layout.leftMargin: contentMargin
        Layout.rightMargin: contentMargin
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignRight
        Layout.rightMargin: contentMargin
        Layout.bottomMargin: MD.Token.spacing.large
        Layout.topMargin: MD.Token.spacing.small
        // Keep footer height stable — toggling visible on «Назад» resizes the
        // BottomSheet (childrenRect) and makes page fades look jumpy.
        Layout.preferredHeight: 40

        MD.Button {
            opacity: stack.depth > 1 ? 1 : 0
            enabled: stack.depth > 1
            mdState.type: MD.Enum.BtText
            text: qsTr("Back")
            onClicked: root.goBack()

            Behavior on opacity {
                NumberAnimation {
                    duration: MD.Token.duration.short4
                    easing: MD.Token.easing.emphasized_decelerate
                }
            }
        }

        MD.Button {
            mdState.type: MD.Enum.BtText
            text: qsTr("Done")
            onClicked: root.closeSheet()
        }
    }
}
