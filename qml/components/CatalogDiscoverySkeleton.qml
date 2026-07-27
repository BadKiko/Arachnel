import QtQuick
import QtQuick.Layouts

import Qcm.Material as MD

ColumnLayout {
    id: root

    required property var page

    spacing: MD.Token.spacing.large

    CatalogShimmerBox {
        Layout.fillWidth: true
        Layout.preferredHeight: 220
        cornerRadius: page.cardRadius
    }

    Row {
        Layout.fillWidth: true
        spacing: MD.Token.spacing.small

        Repeater {
            model: 7

            CatalogShimmerBox {
                width: 132
                height: 92
                cornerRadius: MD.Token.shape.corner.large
            }
        }
    }

    Repeater {
        model: 3

        ColumnLayout {
            Layout.fillWidth: true
            spacing: MD.Token.spacing.small

            CatalogShimmerBox {
                Layout.preferredWidth: 220
                Layout.preferredHeight: 28
                cornerRadius: MD.Token.shape.corner.small
            }

            Row {
                spacing: MD.Token.spacing.medium

                Repeater {
                    model: 6

                    CatalogShimmerBox {
                        width: page.cardWidth
                        height: page.cardHeight
                        cornerRadius: MD.Token.shape.corner.large
                    }
                }
            }
        }
    }
}
