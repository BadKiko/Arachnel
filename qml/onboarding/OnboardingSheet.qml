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
        Core.refreshOfficialPlugins()
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

    function isPluginInstalled(pluginId) {
        if (Core.isPluginInstalledOnDisk(pluginId))
            return true
        for (let i = 0; i < pluginRows.length; ++i) {
            if (pluginRows[i].pluginId === pluginId)
                return true
        }
        return false
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
                    OnboardingBasicsStep {
                        stepId: root.stepId
                    }
                    OnboardingStoragePluginsStep {
                        stepId: root.stepId
                        pluginRows: root.pluginRows
                        isPluginInstalled: root.isPluginInstalled
                        onNextRequested: root.goNext()
                    }
                    OnboardingFinalSteps {
                        stepId: root.stepId
                        onNextRequested: root.goNext()
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
