import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Templates as T

import Arachnel.Core 1.0
import Qcm.Material as MD

Item {
    id: root

    visible: false
    z: 1800

    signal finished()

    readonly property bool onLinux: Qt.platform.os === "linux"
    readonly property var languageOptions: [
        { code: "en", label: "English" },
        { code: "ru", label: "Русский" }
    ]

    // Step ids in display order (Proton only on Linux).
    readonly property var steps: {
        const list = ["welcome", "language", "theme", "storage", "plugins", "updates"]
        if (root.onLinux)
            list.push("proton")
        list.push("done")
        return list
    }

    property int stepIndex: 0
    readonly property string stepId: steps[Math.min(stepIndex, steps.length - 1)] || "welcome"
    readonly property int stepCount: steps.length
    readonly property bool isFirst: stepIndex <= 0
    readonly property bool isLast: stepIndex >= stepCount - 1

    property var pluginRows: []

    function openWizard() {
        stepIndex = 0
        reloadPlugins()
        if (root.onLinux) {
            Core.refreshProtonLatestRelease()
            Core.refreshAvailableProtons()
        }
        visible = true
    }

    function closeWizard() {
        visible = false
    }

    function reloadPlugins() {
        pluginRows = Core.pluginEntries()
    }

    function goNext() {
        if (root.isLast) {
            finish()
            return
        }
        stepIndex = Math.min(stepIndex + 1, stepCount - 1)
    }

    function goBack() {
        stepIndex = Math.max(stepIndex - 1, 0)
    }

    function finish() {
        Core.settings.onboardingCompleted = true
        closeWizard()
        root.finished()
    }

    function skip() {
        finish()
    }

    Connections {
        target: Core
        function onPluginsChanged() {
            root.reloadPlugins()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: MD.Util.transparent(MD.Token.color.scrim, 0.55)

        MouseArea {
            anchors.fill: parent
            // Modal: block clicks to the app behind the wizard.
            onClicked: {}
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 48)
        height: Math.min(640, parent.height - 48)
        radius: MD.Token.shape.corner.extra_large
        color: MD.Token.color.surface_container_high
        border.width: 1
        border.color: MD.Token.color.outline_variant
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.topMargin: MD.Token.spacing.large
                spacing: MD.Token.spacing.small

                SpiderWebMark {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    strokeColor: MD.Token.color.primary
                    strokeWidth: 1.5
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Welcome to Arachnel")
                        typescale: MD.Token.typescale.title_large
                    }

                    MD.Label {
                        Layout.fillWidth: true
                        text: qsTr("Step %1 of %2").arg(root.stepIndex + 1).arg(root.stepCount)
                        color: MD.Token.color.on_surface_variant
                        typescale: MD.Token.typescale.label_medium
                    }
                }

                MD.Button {
                    text: qsTr("Skip")
                    mdState.type: MD.Enum.BtText
                    visible: !root.isLast
                    onClicked: root.skip()
                }
            }

            // Progress dots
            Row {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: MD.Token.spacing.medium
                spacing: 6

                Repeater {
                    model: root.stepCount

                    Rectangle {
                        required property int index
                        width: index === root.stepIndex ? 18 : 8
                        height: 8
                        radius: 4
                        color: index === root.stepIndex
                               ? MD.Token.color.primary
                               : MD.Token.color.outline_variant

                        Behavior on width {
                            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
                        }
                    }
                }
            }

            Flickable {
                id: bodyFlick
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: MD.Token.spacing.medium
                contentWidth: width
                contentHeight: body.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick

                ColumnLayout {
                    id: body
                    width: bodyFlick.width
                    spacing: MD.Token.spacing.medium

                    // —— Welcome ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: MD.Token.spacing.large
                        Layout.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.small
                        visible: root.stepId === "welcome"

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("A quick setup before you start")
                            typescale: MD.Token.typescale.headline_small
                            wrapMode: Text.WordWrap
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("A quick setup: language, storage, plugins, and a few defaults. Change anything later in Settings.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }
                    }

                    // —— Language ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: MD.Token.spacing.large
                        Layout.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.small
                        visible: root.stepId === "language"

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Language")
                            typescale: MD.Token.typescale.headline_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Choose the interface language.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        Repeater {
                            model: root.languageOptions

                            MD.Button {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.label
                                mdState.type: Core.settings.uiLanguage === modelData.code
                                              ? MD.Enum.BtFilled
                                              : MD.Enum.BtOutlined
                                onClicked: Core.settings.uiLanguage = modelData.code
                            }
                        }
                    }

                    // —— Theme ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: MD.Token.spacing.large
                        Layout.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.small
                        visible: root.stepId === "theme"

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Appearance")
                            typescale: MD.Token.typescale.headline_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Pick light or dark theme, palette, and accent color. Change later in Settings.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small

                            MD.Button {
                                Layout.fillWidth: true
                                text: qsTr("Dark")
                                mdState.type: Appearance.themeMode === MD.Enum.Dark
                                              ? MD.Enum.BtFilled
                                              : MD.Enum.BtOutlined
                                onClicked: Appearance.setThemeMode(MD.Enum.Dark)
                            }

                            MD.Button {
                                Layout.fillWidth: true
                                text: qsTr("Light")
                                mdState.type: Appearance.themeMode === MD.Enum.Light
                                              ? MD.Enum.BtFilled
                                              : MD.Enum.BtOutlined
                                onClicked: Appearance.setThemeMode(MD.Enum.Light)
                            }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            Layout.topMargin: MD.Token.spacing.extra_small
                            text: qsTr("Palette")
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.label_large
                        }

                        MD.HorizontalListView {
                            id: onboardingPaletteList
                            Layout.fillWidth: true
                            expand: true
                            spacing: MD.Token.spacing.small
                            implicitHeight: 40
                            model: MD.PaletteModel {}
                            currentIndex: Appearance.paletteType

                            MD.ActionGroup {
                                id: onboardingPaletteGroup
                            }

                            delegate: MD.InputChip {
                                required property int index
                                required property var model

                                action: MD.Action {
                                    T.ActionGroup.group: onboardingPaletteGroup
                                    icon.name: ""
                                    checkable: true
                                    checked: onboardingPaletteList.currentIndex === index
                                    text: model.name
                                    onTriggered: {
                                        onboardingPaletteList.currentIndex = index
                                        Appearance.setPaletteType(index)
                                    }
                                }
                            }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Primary")
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.label_large
                        }

                        Grid {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: MD.Token.spacing.medium
                            rows: 2
                            columns: 6

                            Repeater {
                                model: AccentColors.palette

                                MD.ColorRadio {
                                    required property var modelData
                                    size: 32
                                    color: modelData.color
                                    checked: Appearance.accentColor === modelData.color
                                    onClicked: Appearance.setAccentColor(modelData.color)
                                }
                            }
                        }
                    }

                    // —— Storage ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: MD.Token.spacing.large
                        Layout.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.small
                        visible: root.stepId === "storage"

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Game library folder")
                            typescale: MD.Token.typescale.headline_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Choose where games are installed. Downloads go to a subfolder on the same drive.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        Repeater {
                            model: Core.settings.storageLibraries

                            Rectangle {
                                required property string libraryId
                                required property string label
                                required property string path
                                required property bool isDefault

                                Layout.fillWidth: true
                                radius: MD.Token.shape.corner.medium
                                color: isDefault
                                       ? MD.Token.color.secondary_container
                                       : MD.Token.color.surface_container
                                border.width: 1
                                border.color: isDefault
                                              ? MD.Token.color.primary
                                              : MD.Token.color.outline_variant
                                implicitHeight: libRow.implicitHeight + MD.Token.spacing.small * 2

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: Core.settings.storageLibraries.setDefaultLibrary(libraryId)
                                }

                                RowLayout {
                                    id: libRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: MD.Token.spacing.small
                                    spacing: MD.Token.spacing.small

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: MD.Token.spacing.extra_small

                                            MD.Label {
                                                Layout.fillWidth: true
                                                text: label
                                                typescale: MD.Token.typescale.title_small
                                            }

                                            MD.Icon {
                                                visible: isDefault
                                                name: MD.Token.icon.star
                                                size: 18
                                                color: MD.Token.color.primary
                                            }
                                        }

                                        MD.Label {
                                            Layout.fillWidth: true
                                            text: path
                                            color: MD.Token.color.on_surface_variant
                                            typescale: MD.Token.typescale.body_small
                                            elide: Text.ElideMiddle
                                        }
                                    }
                                }
                            }
                        }

                        MD.Button {
                            Layout.fillWidth: true
                            mdState.type: MD.Enum.BtOutlined
                            text: qsTr("Choose folder…")
                            icon.name: MD.Token.icon.folder_open
                            onClicked: {
                                const folder = Core.browseStorageFolder()
                                if (!folder.length)
                                    return
                                const id = Core.settings.storageLibraries.addLibrary(folder)
                                if (id.length)
                                    Core.settings.storageLibraries.setDefaultLibrary(id)
                            }
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Or keep the default path already listed above.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_small
                        }
                    }

                    // —— Plugins ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: MD.Token.spacing.large
                        Layout.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.small
                        visible: root.stepId === "plugins"

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Source plugins")
                            typescale: MD.Token.typescale.headline_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Plugins enable automatic install and Play (e.g. FreeTP). Without one, you can still browse catalogs and install manually.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: MD.Token.shape.corner.large
                            color: MD.Token.color.surface_container
                            border.width: 1
                            border.color: MD.Token.color.outline_variant
                            implicitHeight: officialCol.implicitHeight + MD.Token.spacing.medium * 2

                            ColumnLayout {
                                id: officialCol
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: MD.Token.spacing.medium
                                spacing: MD.Token.spacing.extra_small

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Official plugins")
                                    typescale: MD.Token.typescale.title_small
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Official plugins are coming soon. For now, install a plugin file you already have (e.g. FreeTP).")
                                    wrapMode: Text.WordWrap
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_small
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.small
                            visible: root.pluginRows.length > 0

                            Repeater {
                                model: root.pluginRows

                                Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    radius: MD.Token.shape.corner.medium
                                    color: MD.Token.color.surface_container
                                    border.width: 1
                                    border.color: MD.Token.color.outline_variant
                                    implicitHeight: plugRow.implicitHeight + MD.Token.spacing.small * 2

                                    RowLayout {
                                        id: plugRow
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: MD.Token.spacing.small
                                        spacing: MD.Token.spacing.small

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            MD.Label {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                typescale: MD.Token.typescale.title_small
                                            }

                                            MD.Label {
                                                Layout.fillWidth: true
                                                text: qsTr("v%1 · %2").arg(modelData.pluginVersion).arg(modelData.pluginId)
                                                color: MD.Token.color.on_surface_variant
                                                typescale: MD.Token.typescale.label_small
                                            }
                                        }

                                        MD.Switch {
                                            checked: modelData.sourceEnabled
                                            onToggled: Core.sources.setSourceEnabled(modelData.pluginId, checked)
                                        }
                                    }
                                }
                            }
                        }

                        MD.Button {
                            Layout.fillWidth: true
                            mdState.type: MD.Enum.BtFilled
                            text: qsTr("Install plugin…")
                            onClicked: Core.browsePluginArach()
                        }

                        MD.Button {
                            Layout.fillWidth: true
                            mdState.type: MD.Enum.BtText
                            text: qsTr("Skip for now")
                            onClicked: root.goNext()
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: Core.lastPluginError.length > 0
                            text: Core.lastPluginError
                            color: MD.Token.color.error
                            wrapMode: Text.WordWrap
                            typescale: MD.Token.typescale.body_small
                        }
                    }

                    // —— Updates ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: MD.Token.spacing.large
                        Layout.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.medium
                        visible: root.stepId === "updates"

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Updates")
                            typescale: MD.Token.typescale.headline_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Recommended defaults — change anytime in Settings → Updates.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.medium

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Check for game updates")
                                    typescale: MD.Token.typescale.body_large
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Notify you when a newer build is available.")
                                    wrapMode: Text.WordWrap
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_small
                                }
                            }

                            MD.Switch {
                                checked: Core.settings.autoCheckUpdates
                                onToggled: Core.settings.autoCheckUpdates = checked
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: MD.Token.spacing.medium

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Check for Arachnel updates")
                                    typescale: MD.Token.typescale.body_large
                                }

                                MD.Label {
                                    Layout.fillWidth: true
                                    text: qsTr("Check for new Arachnel versions automatically.")
                                    wrapMode: Text.WordWrap
                                    color: MD.Token.color.on_surface_variant
                                    typescale: MD.Token.typescale.body_small
                                }
                            }

                            MD.Switch {
                                checked: Core.settings.autoCheckAppUpdates
                                onToggled: Core.settings.autoCheckAppUpdates = checked
                            }
                        }
                    }

                    // —— Proton (Linux) ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: MD.Token.spacing.large
                        Layout.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.small
                        visible: root.stepId === "proton"

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Proton (Linux)")
                            typescale: MD.Token.typescale.headline_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Windows games need Proton on Linux. Install it now or later in Settings → Launch.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: Core.protonReady
                            text: qsTr("Proton ready: %1").arg(Core.protonVersion)
                            color: MD.Token.color.primary
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            visible: Core.protonDownloadInProgress
                            text: Core.protonDownloadStatus.length
                                  ? (Core.protonDownloadStatus + " (" + Core.protonDownloadProgress + "%)")
                                  : qsTr("Downloading Proton… %1%").arg(Core.protonDownloadProgress)
                            wrapMode: Text.WordWrap
                            typescale: MD.Token.typescale.body_small
                        }

                        MD.Button {
                            Layout.fillWidth: true
                            mdState.type: MD.Enum.BtFilled
                            enabled: !Core.protonDownloadInProgress
                            text: Core.protonReady
                                  ? qsTr("Proton already installed")
                                  : (Core.protonLatestRelease.length
                                     ? qsTr("Download Proton-GE %1").arg(Core.protonLatestRelease)
                                     : qsTr("Download Proton-GE"))
                            onClicked: {
                                if (!Core.protonReady)
                                    Core.downloadProtonGe()
                            }
                        }

                        MD.Button {
                            Layout.fillWidth: true
                            mdState.type: MD.Enum.BtText
                            text: qsTr("I'll do this later")
                            enabled: !Core.protonDownloadInProgress
                            onClicked: root.goNext()
                        }
                    }

                    // —— Done ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: MD.Token.spacing.large
                        Layout.rightMargin: MD.Token.spacing.large
                        spacing: MD.Token.spacing.small
                        visible: root.stepId === "done"

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("You're all set")
                            typescale: MD.Token.typescale.headline_small
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Open Catalog to browse games. Change language, storage, and plugins anytime in Settings.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_medium
                        }

                        MD.Label {
                            Layout.fillWidth: true
                            text: qsTr("Tip: with a plugin installed, Install runs automatically after download. Catalog-only setups need a manual Install step.")
                            wrapMode: Text.WordWrap
                            color: MD.Token.color.on_surface_variant
                            typescale: MD.Token.typescale.body_small
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: MD.Token.spacing.large
                Layout.rightMargin: MD.Token.spacing.large
                Layout.bottomMargin: MD.Token.spacing.large
                Layout.topMargin: MD.Token.spacing.small
                spacing: MD.Token.spacing.small

                MD.Button {
                    text: qsTr("Back")
                    mdState.type: MD.Enum.BtOutlined
                    enabled: !root.isFirst
                    onClicked: root.goBack()
                }

                Item { Layout.fillWidth: true }

                MD.Button {
                    text: root.isLast ? qsTr("Get started") : qsTr("Next")
                    mdState.type: MD.Enum.BtFilled
                    onClicked: root.goNext()
                }
            }
        }
    }
}
