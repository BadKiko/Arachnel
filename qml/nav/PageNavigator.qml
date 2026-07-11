import QtQuick
import QtQuick.Controls

import Qcm.Material as MD

// Unified page navigation: fade + bounce scale (OutBack).
// Covered pages can stay at opacity 0 after Immediate pop; restoreCurrent() fixes that.
MD.StackView {
    id: root

    readonly property bool canPop: depth > 1
    readonly property int enterDuration: MD.Token.duration.medium4
    readonly property int exitDuration: MD.Token.duration.short3

    clip: true

    pushEnter: PageEnterMotion {}
    pushExit: PageExitMotion {}
    popEnter: PageEnterMotion {}
    popExit: PageExitMotion {}
    replaceEnter: PageEnterMotion {}
    replaceExit: PageExitMotion {}

    function restoreCurrent() {
        const item = currentItem
        if (!item)
            return
        item.opacity = 1
        item.scale = 1
        if (item.visible !== undefined)
            item.visible = true
    }

    function restoreItem(item) {
        if (!item)
            return
        item.opacity = 1
        item.scale = 1
        if (item.visible !== undefined)
            item.visible = true
    }

    onBusyChanged: {
        if (!busy)
            restoreCurrent()
    }

    function navigatePush(component, properties, immediate) {
        const item = immediate
            ? push(component, properties || {}, StackView.Immediate)
            : push(component, properties || {})
        if (immediate && !busy)
            restoreCurrent()
        return item
    }

    function navigatePop(immediate) {
        if (depth <= 1)
            return null
        const below = depth >= 2 ? get(depth - 2) : null
        const result = immediate ? pop(StackView.Immediate) : pop()
        if (!busy || immediate)
            restoreItem(below)
        return result
    }

    function navigatePopTo(item, immediate) {
        if (!item || depth <= 1)
            return null
        const result = immediate ? pop(item, StackView.Immediate) : pop(item)
        if (!busy || immediate)
            restoreItem(item)
        return result
    }

    function navigateReplace(component, properties, immediate) {
        if (immediate)
            return replace(currentItem, component, properties || {}, StackView.Immediate)
        return replace(currentItem, component, properties || {})
    }

    function navigateReset(component, properties) {
        clear(StackView.Immediate)
        const item = push(component, properties || {}, StackView.Immediate)
        restoreCurrent()
        return item
    }

    function navigateToRoot(immediate) {
        if (depth <= 1) {
            restoreCurrent()
            return currentItem
        }
        const rootItem = get(0)
        const result = immediate ? pop(rootItem, StackView.Immediate) : pop(rootItem)
        if (!busy || immediate)
            restoreItem(rootItem)
        return result
    }
}
