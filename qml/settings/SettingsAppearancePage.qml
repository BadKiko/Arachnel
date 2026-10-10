import QtQuick
import QtQuick.Layouts

import Arachnel.Core 1.0
import Qcm.Material as MD

Flickable {
    id: root

    property int contentMargin: MD.Token.spacing.large
    property bool applying: false

    readonly property var languageOptions: [
        { code: "en", label: qsTr("English") },
        { code: "ru", label: qsTr("Russian") }
    ]

    readonly property string weblateTranslateUrl: "https://hosted.weblate.org/projects/arachnel/application/"

    contentWidth: width
    contentHeight: body.implicitHeight + MD.Token.spacing.extra_large
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    function syncFromStore() {
        applying = true
        Appearance.apply()
        applying = false
    }

    Component.onCompleted: syncFromStore()

    SettingsColumn {
        id: body
        margin: root.contentMargin

        SettingsGroup {
            title: qsTr("Theme")
            description: qsTr("Theme and colors apply across the app.")

            SettingsSwitchRow {
                title: qsTr("Dark theme")
                iconName: MD.Token.icon.dark_mode
                checked: MD.Token.isDarkTheme
                onToggled: function (on) {
                    if (!root.applying)
                        Appearance.setThemeMode(on ? MD.Enum.Dark : MD.Enum.Light)
                }
            }

            SettingsSwitchRow {
                title: qsTr("Reduce motion")
                iconName: MD.Token.icon.animation
                checked: Appearance.reduceMotion
                onToggled: function (on) {
                    if (!root.applying)
                        Appearance.setReduceMotion(on)
                }
            }
        }

        SettingsGroup {
            title: qsTr("Colors")

            SettingsRow {
                title: qsTr("App colour")
                subtitle: qsTr("Each circle shows the colours the app will use.")
                iconName: MD.Token.icon.palette

                below: ThemeColorPicker {
                    Layout.fillWidth: true
                    Layout.topMargin: MD.Token.spacing.small
                    applying: root.applying
                }
            }
        }

        SettingsGroup {
            title: qsTr("Language")

            SettingsRow {
                title: qsTr("Interface language")
                iconName: MD.Token.icon.language

                ConnectedTextToggle {
                    labels: root.languageOptions.map(o => o.label)
                    currentIndex: Math.max(0, root.languageOptions.findIndex(o => o.code === Core.settings.uiLanguage))
                    onPicked: function (index) { Core.settings.uiLanguage = root.languageOptions[index].code }
                }
            }

            SettingsRow {
                title: qsTr("Community translations")
                subtitle: qsTr("Missing your language? Help translate Arachnel on Weblate.")
                iconName: MD.Token.icon.translate
                clickable: true
                showChevron: true
                onClicked: Core.openExternalUrl(root.weblateTranslateUrl)
            }
        }
    }
}
