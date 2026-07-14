import QtQuick

import Qcm.Material as MD

// Material Symbols has no spider-web glyph — draw a simple web with M3 colors.
Item {
    id: root

    property color strokeColor: MD.Token.color.primary
    property real strokeWidth: 2.5
    property int rings: 4
    property int spokes: 8

    implicitWidth: 280
    implicitHeight: 280

    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    onStrokeColorChanged: canvas.requestPaint()
    onStrokeWidthChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        renderTarget: Canvas.FramebufferObject
        renderStrategy: Canvas.Cooperative

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const cx = width / 2
            const cy = height / 2
            const maxR = Math.min(cx, cy) * 0.92

            ctx.strokeStyle = root.strokeColor
            ctx.lineWidth = root.strokeWidth
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            for (let s = 0; s < root.spokes; ++s) {
                const a = (s / root.spokes) * Math.PI * 2 - Math.PI / 2
                ctx.beginPath()
                ctx.moveTo(cx, cy)
                ctx.lineTo(cx + Math.cos(a) * maxR, cy + Math.sin(a) * maxR)
                ctx.stroke()
            }

            for (let r = 1; r <= root.rings; ++r) {
                const rad = maxR * (r / root.rings)
                ctx.beginPath()
                for (let s = 0; s <= root.spokes; ++s) {
                    const a = (s / root.spokes) * Math.PI * 2 - Math.PI / 2
                    const x = cx + Math.cos(a) * rad
                    const y = cy + Math.sin(a) * rad
                    if (s === 0)
                        ctx.moveTo(x, y)
                    else
                        ctx.lineTo(x, y)
                }
                ctx.closePath()
                ctx.stroke()
            }

            ctx.beginPath()
            ctx.arc(cx, cy, Math.max(3, root.strokeWidth * 1.6), 0, Math.PI * 2)
            ctx.fillStyle = root.strokeColor
            ctx.fill()
        }

        Component.onCompleted: requestPaint()
    }
}
