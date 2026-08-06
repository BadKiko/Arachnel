---
description: Always design Windows+Linux/Proton paths - Arachnel is cross-platform
alwaysApply: true
---

# Cross-platform + Proton

Arachnel runs on **Windows and Linux**. On Linux, Windows games launch under **Proton** (Wine prefix per game). Do not ship Windows-only fixes.

## When designing or coding

1. Ask: what happens on Linux / Proton for the same user action?
2. Prefer one code path that works on both hosts. Gate with `Q_OS_LINUX` / `Q_OS_WIN` only when the mechanism must differ.
3. Online-fix / SteamFix / winmm / FreeTP are **Windows PE** artifacts. On Linux they still apply inside Proton - plant files next to the `.exe`, set `WINEDLLOVERRIDES`, use `STEAM_COMPAT_DATA_PATH`. Do not assume native Linux APIs replace them.
4. Tricks that rely on Win32 CreateProcess search (e.g. noop `cmd.exe` next to the game) must also be planted for Proton launches - same files on disk, verified via the Linux launch path (`applyOnlineFixLaunchInfo` / `launchInfo` repair).
5. Do not replace Wine `system32/cmd.exe` in the Proton prefix - that breaks `proton run` / installers. Prefer stubs beside the game exe / overlay dir.
6. Test or reason about both: native Windows launch and `proton run game.exe`.

## Examples

```text
❌ "Fixed FreeTP browser spam" - only Overlay=false on Windows
✅ plant FreeTP ack + cmd stub on embed AND on every launch (Windows + Proton)

❌ skip Linux because SteamFix is a DLL
✅ DLL loads under Proton; host just copies PE files and sets Wine env
```
