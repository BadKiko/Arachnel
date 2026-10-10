import QtQuick
import QtQuick.Shapes

import Qcm.Material as MD

// A Material 3 Expressive shape (circle, cookie, clover, sunny, burst...) drawn as a smooth polar
// curve. `morph` scales the lobes, so a shape can soften into a circle or sharpen on hover.
Shape {
    id: root

    // "circle" | "cookie4" | "cookie6" | "cookie9" | "cookie12" | "clover4" | "clover8" | "sunny" | "burst"
    property string kind: "cookie9"
    property color color: MD.Token.color.primary_container
    property real morph: 1.0

    readonly property var spec: {
        switch (root.kind) {
        case "circle": return { n: 1, depth: 0 }
        case "cookie4": return { n: 4, depth: 0.10 }
        case "cookie6": return { n: 6, depth: 0.09 }
        case "cookie9": return { n: 9, depth: 0.075 }
        case "cookie12": return { n: 12, depth: 0.06 }
        case "clover4": return { n: 4, depth: 0.17 }
        case "clover8": return { n: 8, depth: 0.12 }
        case "sunny": return { n: 8, depth: 0.10 }
        case "burst": return { n: 12, depth: 0.16 }
        }
        return { n: 9, depth: 0.075 }
    }

    readonly property var points: {
        const n = spec.n
        const depth = Math.min(0.2, spec.depth * Math.max(0, root.morph))
        const cx = width / 2
        const cy = height / 2
        const radius = Math.min(width, height) / 2
        const steps = 120
        const pts = []
        for (let i = 0; i <= steps; ++i) {
            const t = i / steps * Math.PI * 2
            // Smooth lobes: 1 at the tips, 1 - depth*2 in the valleys.
            const r = radius * (1 - depth * (1 - Math.cos(n * t)))
            pts.push(Qt.point(cx + r * Math.cos(t - Math.PI / 2), cy + r * Math.sin(t - Math.PI / 2)))
        }
        return pts
    }

    preferredRendererType: Shape.CurveRenderer
    antialiasing: true

    ShapePath {
        fillColor: root.color
        strokeWidth: -1
        PathPolyline { path: root.points }
    }
}
