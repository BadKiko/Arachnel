#include "snap_layouts_filter.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>
#include <QRectF>
#include <QVariant>
#include <QWindow>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <windowsx.h>
#endif

namespace arachnel {

#if defined(Q_OS_WIN)

namespace {

class SnapLayoutsFilter final : public QObject, public QAbstractNativeEventFilter {
public:
    SnapLayoutsFilter(QWindow* window, QObject* titleBar)
        : QObject(window)
        , m_window(window)
        , m_titleBar(titleBar)
    {
    }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override
    {
        if (eventType != "windows_generic_MSG" || !m_window || !m_titleBar)
            return false;
        // QWindow::winId() creates the platform window when it does not exist. While the main
        // window is being torn down that means a CreateWindowEx per native message, each one
        // producing more messages: the process spun for ~30 s after the window had closed.
        if (!m_window->handle())
            return false;

        MSG* msg = static_cast<MSG*>(message);
        if (msg->hwnd != reinterpret_cast<HWND>(m_window->winId()))
            return false;

        switch (msg->message) {
        case WM_NCHITTEST: {
            POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            ScreenToClient(msg->hwnd, &pt);
            const bool over = isOverMaxButton(pt);
            setBarFlag(m_hovered, "maxButtonHovered", over);
            if (!over)
                return false;
            // Without this the hover state sticks when the cursor leaves the window.
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE | TME_NONCLIENT, msg->hwnd, 0};
            TrackMouseEvent(&track);
            if (result)
                *result = HTMAXBUTTON;
            return true;
        }
        case WM_NCCALCSIZE:
            // The window now has a frame (needed for Snap Layouts) but we draw everything in
            // QML: keep the whole window as client area, except the part a maximized window
            // spills over the screen edge.
            if (msg->wParam == TRUE) {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
                if (IsZoomed(msg->hwnd)) {
                    const UINT dpi = GetDpiForWindow(msg->hwnd);
                    const int frame = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi)
                                      + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                    const int frameY = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi)
                                       + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                    params->rgrc[0].left += frame;
                    params->rgrc[0].right -= frame;
                    params->rgrc[0].top += frameY;
                    params->rgrc[0].bottom -= frameY;
                }
                if (result)
                    *result = 0;
                return true;
            }
            return false;
        case WM_NCMOUSELEAVE:
            setBarFlag(m_hovered, "maxButtonHovered", false);
            setBarFlag(m_pressed, "maxButtonPressed", false);
            return false;
        case WM_NCLBUTTONDOWN:
            if (msg->wParam == HTMAXBUTTON) {
                setBarFlag(m_pressed, "maxButtonPressed", true);
                if (result)
                    *result = 0;
                return true;
            }
            return false;
        case WM_NCLBUTTONUP:
            if (msg->wParam == HTMAXBUTTON) {
                const bool wasPressed = m_pressed;
                setBarFlag(m_pressed, "maxButtonPressed", false);
                if (wasPressed)
                    QMetaObject::invokeMethod(m_titleBar, "toggleMaximize");
                if (result)
                    *result = 0;
                return true;
            }
            setBarFlag(m_pressed, "maxButtonPressed", false);
            return false;
        default:
            return false;
        }
    }

private:
    bool isOverMaxButton(const POINT& clientPx) const
    {
        const QRectF rect = m_titleBar->property("maxButtonRect").toRectF();
        if (rect.isEmpty())
            return false;
        const qreal dpr = m_window->devicePixelRatio();
        return rect.contains(QPointF(clientPx.x / dpr, clientPx.y / dpr));
    }

    void setBarFlag(bool& cache, const char* name, bool value)
    {
        if (cache == value)
            return;
        cache = value;
        m_titleBar->setProperty(name, value);
    }

    QPointer<QWindow> m_window;
    QPointer<QObject> m_titleBar;
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace

void installSnapLayoutsFilter(QWindow* window, QObject* titleBar)
{
    if (!window || !titleBar)
        return;
    // A frameless Qt window is a bare WS_POPUP; the shell only offers Snap Layouts for
    // windows that have a maximize box.
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE,
                      style | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME | WS_CAPTION);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Parented to the window so it is removed from the app before the window goes away.
    auto* filter = new SnapLayoutsFilter(window, titleBar);
    QCoreApplication::instance()->installNativeEventFilter(filter);
    QObject::connect(window, &QObject::destroyed, filter, [filter]() {
        QCoreApplication::instance()->removeNativeEventFilter(filter);
    });
}

#else

void installSnapLayoutsFilter(QWindow*, QObject*)
{
}

#endif

} // namespace arachnel
