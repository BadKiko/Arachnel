pragma Singleton

import QtQml

QtObject {
    readonly property var palette: [
        { name: "Gray", color: "#8E8E93" },
        { name: "Unicase", color: "#984300" },
        { name: "Red", color: "#F44336" },
        { name: "Pink", color: "#E91E63" },
        { name: "Purple", color: "#9C27B0" },
        { name: "Indigo", color: "#3F51B5" },
        { name: "Teal", color: "#009688" },
        { name: "LightGreen", color: "#8BC34A" },
        { name: "Yellow", color: "#FFEB3B" },
        { name: "Amber", color: "#FFC107" },
        { name: "Orange", color: "#FF9800" }
    ]
}
