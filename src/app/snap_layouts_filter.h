#pragma once

#include <QObject>

class QWindow;

namespace arachnel {

/**
 * Windows 11 shows the Snap Layouts flyout only when the maximize button answers
 * WM_NCHITTEST with HTMAXBUTTON. Our title bar is drawn in QML (frameless window), so
 * this filter reports the QML maximize button as HTMAXBUTTON and drives its hover /
 * pressed state and click itself, because the native side swallows those mouse events.
 *
 * `titleBar` is the QML item named "appTitleBar" (see AppTitleBar.qml): it exposes
 * `maxButtonRect`, `maxButtonHovered`, `maxButtonPressed` and `toggleMaximize()`.
 * No-op on non-Windows platforms.
 */
void installSnapLayoutsFilter(QWindow* window, QObject* titleBar);

} // namespace arachnel
