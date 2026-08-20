#include "wine_error_probe.h"

#include "process_tracker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QString>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#elif defined(ARACHNEL_HAVE_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

namespace arachnel::core {

namespace {

bool titleLooksLikeError(const QString& title)
{
    const QString t = title.trimmed();
    if (t.isEmpty())
        return false;
    const QString lower = t.toLower();
    return lower == QLatin1String("error")
        || lower.contains(QLatin1String("error code"))
        || lower.contains(QLatin1String("fatal error"))
        || lower.contains(QLatin1String("self-protection"))
        || lower.contains(QLatin1String("protection failed"))
        || lower.contains(QStringLiteral("ошибка"));
}

QString normalizePathHint(QString path)
{
    path = QDir::cleanPath(path.trimmed());
    while (path.endsWith(QLatin1Char('/')) && path.size() > 1)
        path.chop(1);
    return path;
}

bool cmdlineMatchesHints(const QByteArray& cmdline, const WineErrorWatchHints& hints)
{
    if (cmdline.isEmpty())
        return false;
    const QByteArray lower = cmdline.toLower();
    const QString install = normalizePathHint(hints.installPath);
    if (!install.isEmpty()) {
        const QByteArray needle = QFile::encodeName(install).toLower();
        if (!needle.isEmpty() && lower.contains(needle))
            return true;
        // Wine often uses Z:\home\... with backslashes.
        QByteArray winePath = needle;
        winePath.replace('/', '\\');
        if (!winePath.isEmpty() && lower.contains(winePath))
            return true;
    }
    const QString exe = hints.executableName.trimmed();
    if (!exe.isEmpty()) {
        const QByteArray needle = QFile::encodeName(exe).toLower();
        if (!needle.isEmpty() && lower.contains(needle))
            return true;
    }
    return false;
}

#if defined(Q_OS_WIN)
QByteArray winProcessImagePath(DWORD pid)
{
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!handle)
        return {};
    wchar_t buf[MAX_PATH * 4] = {};
    DWORD size = MAX_PATH * 4;
    QByteArray out;
    if (QueryFullProcessImageNameW(handle, 0, buf, &size))
        out = QString::fromWCharArray(buf, int(size)).toUtf8();
    CloseHandle(handle);
    return out;
}
#endif

#if !defined(Q_OS_WIN)
QByteArray linuxCmdline(qint64 pid)
{
    QFile file(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

QString linuxCwd(qint64 pid)
{
    return QFileInfo(QStringLiteral("/proc/%1/cwd").arg(pid)).symLinkTarget();
}
#endif

QSet<qint64> relatedPidSet(qint64 launchProcessId, const WineErrorWatchHints& hints)
{
    QSet<qint64> set;
    for (qint64 pid : ProcessTracker::processTreePids(launchProcessId))
        set.insert(pid);

#if defined(Q_OS_WIN)
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snap, &entry)) {
            do {
                const qint64 pid = static_cast<qint64>(entry.th32ProcessID);
                if (set.contains(pid))
                    continue;
                const QString exeName = QString::fromWCharArray(entry.szExeFile);
                if (!hints.executableName.isEmpty()
                    && exeName.compare(hints.executableName, Qt::CaseInsensitive) == 0) {
                    set.insert(pid);
                    continue;
                }
                if (cmdlineMatchesHints(winProcessImagePath(entry.th32ProcessID), hints))
                    set.insert(pid);
            } while (Process32NextW(snap, &entry));
        }
        CloseHandle(snap);
    }
#else
    const QDir proc(QStringLiteral("/proc"));
    const QStringList names = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    const QString install = normalizePathHint(hints.installPath);
    for (const QString& name : names) {
        bool ok = false;
        const qint64 pid = name.toLongLong(&ok);
        if (!ok || pid <= 1 || set.contains(pid))
            continue;
        if (cmdlineMatchesHints(linuxCmdline(pid), hints)) {
            set.insert(pid);
            continue;
        }
        if (!install.isEmpty()) {
            const QString cwd = normalizePathHint(linuxCwd(pid));
            if (!cwd.isEmpty()
                && (cwd == install || cwd.startsWith(install + QLatin1Char('/'))))
                set.insert(pid);
        }
    }
#endif
    return set;
}

bool classMatchesHints(const QString& wmClass, const WineErrorWatchHints& hints)
{
    if (wmClass.isEmpty())
        return false;
    const QString lower = wmClass.toLower();
    if (!hints.fakeSteamAppId.isEmpty()) {
        const QString steamClass =
            QStringLiteral("steam_app_%1").arg(hints.fakeSteamAppId.trimmed());
        if (lower.contains(steamClass.toLower()))
            return true;
    }
    QString exe = hints.executableName.trimmed();
    if (exe.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive))
        exe.chop(4);
    if (!exe.isEmpty() && lower.contains(exe.toLower()))
        return true;
    return false;
}

#if defined(Q_OS_WIN)
struct EnumCtx {
    const QSet<qint64>* pids = nullptr;
    const WineErrorWatchHints* hints = nullptr;
    bool found = false;
};

BOOL CALLBACK enumDialogWindows(HWND hwnd, LPARAM lparam)
{
    auto* ctx = reinterpret_cast<EnumCtx*>(lparam);
    if (!ctx || !ctx->pids || ctx->found)
        return FALSE;
    if (!IsWindowVisible(hwnd))
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    const bool pidHit = ctx->pids->contains(static_cast<qint64>(pid));

    wchar_t cls[128] = {};
    GetClassNameW(hwnd, cls, 128);
    wchar_t title[512] = {};
    GetWindowTextW(hwnd, title, 512);
    const QString titleStr = QString::fromWCharArray(title);
    const QString classStr = QString::fromWCharArray(cls);
    const bool errorTitle = titleLooksLikeError(titleStr);
    const bool classHit = ctx->hints && classMatchesHints(classStr, *ctx->hints);

    // MessageBox class, or error-titled window owned by related pid / steam_app class.
    if (wcscmp(cls, L"#32770") == 0 && (pidHit || classHit || errorTitle)) {
        ctx->found = true;
        return FALSE;
    }
    if (errorTitle && (pidHit || classHit)) {
        ctx->found = true;
        return FALSE;
    }
    return TRUE;
}
#endif

#if defined(ARACHNEL_HAVE_X11)
bool windowIsDialog(Display* dpy, Window win, Atom typeAtom, Atom dialogAtom)
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long nitems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* prop = nullptr;
    if (XGetWindowProperty(dpy, win, typeAtom, 0, 16, False, XA_ATOM, &actualType, &actualFormat,
                           &nitems, &bytesAfter, &prop)
            != Success
        || !prop || actualFormat != 32 || nitems < 1) {
        if (prop)
            XFree(prop);
        return false;
    }
    bool hit = false;
    const auto* atoms = reinterpret_cast<const Atom*>(prop);
    for (unsigned long i = 0; i < nitems; ++i) {
        if (atoms[i] == dialogAtom) {
            hit = true;
            break;
        }
    }
    XFree(prop);
    return hit;
}

qint64 windowPid(Display* dpy, Window win, Atom pidAtom)
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long nitems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* prop = nullptr;
    if (XGetWindowProperty(dpy, win, pidAtom, 0, 1, False, XA_CARDINAL, &actualType, &actualFormat,
                           &nitems, &bytesAfter, &prop)
            != Success
        || !prop || actualFormat != 32 || nitems < 1) {
        if (prop)
            XFree(prop);
        return 0;
    }
    const auto pid = static_cast<qint64>(*reinterpret_cast<const unsigned long*>(prop));
    XFree(prop);
    return pid;
}

QString windowTitle(Display* dpy, Window win, Atom netNameAtom)
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long nitems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* prop = nullptr;
    if (XGetWindowProperty(dpy, win, netNameAtom, 0, 256, False, AnyPropertyType, &actualType,
                           &actualFormat, &nitems, &bytesAfter, &prop)
            == Success
        && prop && nitems > 0) {
        QString title;
        if (actualFormat == 8)
            title = QString::fromUtf8(reinterpret_cast<const char*>(prop), int(nitems));
        else if (actualFormat == 16)
            title = QString::fromUtf16(reinterpret_cast<const char16_t*>(prop), int(nitems));
        XFree(prop);
        if (!title.isEmpty())
            return title;
    } else if (prop) {
        XFree(prop);
    }

    XTextProperty text = {};
    if (XGetWMName(dpy, win, &text) && text.value) {
        char** list = nullptr;
        int count = 0;
        if (Xutf8TextPropertyToTextList(dpy, &text, &list, &count) >= Success && count > 0 && list
            && list[0]) {
            const QString title = QString::fromUtf8(list[0]);
            XFreeStringList(list);
            if (text.value)
                XFree(text.value);
            return title;
        }
        if (list)
            XFreeStringList(list);
        if (text.value)
            XFree(text.value);
    }
    return {};
}

QString windowClass(Display* dpy, Window win)
{
    XClassHint hint = {};
    if (!XGetClassHint(dpy, win, &hint))
        return {};
    QString out;
    if (hint.res_class)
        out = QString::fromLocal8Bit(hint.res_class);
    else if (hint.res_name)
        out = QString::fromLocal8Bit(hint.res_name);
    if (hint.res_name)
        XFree(hint.res_name);
    if (hint.res_class)
        XFree(hint.res_class);
    return out;
}

bool scanWindowTree(Display* dpy, Window win, const QSet<qint64>& pids,
                    const WineErrorWatchHints& hints, Atom pidAtom, Atom typeAtom, Atom dialogAtom,
                    Atom netNameAtom, int depth)
{
    if (depth > 24)
        return false;

    const qint64 pid = windowPid(dpy, win, pidAtom);
    const bool pidHit = pid > 0 && pids.contains(pid);
    const QString title = windowTitle(dpy, win, netNameAtom);
    const QString cls = windowClass(dpy, win);
    const bool errorTitle = titleLooksLikeError(title);
    const bool classHit = classMatchesHints(cls, hints);
    const bool dialog = typeAtom != None && dialogAtom != None
        && windowIsDialog(dpy, win, typeAtom, dialogAtom);

    // Wine MessageBox is often _NET_WM_WINDOW_TYPE_NORMAL with title "Error".
    if ((dialog || errorTitle) && (pidHit || classHit))
        return true;

    Window root = 0;
    Window parent = 0;
    Window* children = nullptr;
    unsigned int nchildren = 0;
    if (!XQueryTree(dpy, win, &root, &parent, &children, &nchildren))
        return false;
    bool found = false;
    for (unsigned int i = 0; i < nchildren && !found; ++i) {
        found = scanWindowTree(dpy, children[i], pids, hints, pidAtom, typeAtom, dialogAtom,
                               netNameAtom, depth + 1);
    }
    if (children)
        XFree(children);
    return found;
}
#endif

} // namespace

QList<qint64> relatedLaunchPids(qint64 launchProcessId, const WineErrorWatchHints& hints)
{
    const QSet<qint64> set = relatedPidSet(launchProcessId, hints);
    return QList<qint64>(set.constBegin(), set.constEnd());
}

bool relatedGameExecutableAlive(qint64 launchProcessId, const WineErrorWatchHints& hints)
{
    const QString exe = hints.executableName.trimmed();
    if (exe.isEmpty())
        return false;
    const QByteArray needle = QFile::encodeName(exe).toLower();
    if (needle.isEmpty())
        return false;

    for (qint64 pid : relatedPidSet(launchProcessId, hints)) {
#if defined(Q_OS_WIN)
        const QByteArray image = winProcessImagePath(static_cast<DWORD>(pid)).toLower();
        if (image.contains(needle))
            return true;
#else
        if (linuxCmdline(pid).toLower().contains(needle))
            return true;
#endif
    }
    return false;
}

bool wineErrorDialogVisible(qint64 launchProcessId, const WineErrorWatchHints& hints)
{
    const QSet<qint64> pids = relatedPidSet(launchProcessId, hints);
    if (pids.isEmpty() && hints.fakeSteamAppId.isEmpty() && hints.executableName.isEmpty())
        return false;

#if defined(Q_OS_WIN)
    EnumCtx ctx{&pids, &hints, false};
    EnumWindows(enumDialogWindows, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
#elif defined(ARACHNEL_HAVE_X11)
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy)
        return false;
    const Window root = DefaultRootWindow(dpy);
    const Atom pidAtom = XInternAtom(dpy, "_NET_WM_PID", True);
    const Atom typeAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", True);
    const Atom dialogAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", True);
    const Atom netNameAtom = XInternAtom(dpy, "_NET_WM_NAME", True);
    bool found = false;
    if (pidAtom != None || !hints.fakeSteamAppId.isEmpty() || !hints.executableName.isEmpty()) {
        found = scanWindowTree(dpy, root, pids, hints, pidAtom, typeAtom, dialogAtom, netNameAtom,
                               0);
    }
    XCloseDisplay(dpy);
    return found;
#else
    Q_UNUSED(launchProcessId);
    Q_UNUSED(hints);
    return false;
#endif
}

} // namespace arachnel::core
