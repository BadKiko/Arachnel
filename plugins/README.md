# Плагины источников

Плагины живут в **отдельных репозиториях**. В этом репозитории только хост и SDK.

**Полная инструкция для разработчиков:** [docs/PLUGIN_SDK.md](../docs/PLUGIN_SDK.md)  
(клонирование, сборка, куда копировать, `.arach`, новый плагин с нуля, ABI, отладка)

## Статус

| Плагин | Репозиторий | Статус |
|--------|-------------|--------|
| `freetp` | [arachnel-plugin-freetp](https://github.com/PetWork/arachnel-plugin-freetp) | реализован (torrent → install) |
| `steamidra` | [arachnel-plugin-steamidra](https://gitlab.com/BadKiko/arachnel-plugin-steamidra) | реализован (API v3, `owns_download`, GPL-3) |
| `online-fix` | — | не начат |

Hydra-каталоги (только JSON по URL, без своего install-пайплайна) настраиваются в UI: **Настройки → Каталоги Hydra**.

## SteaMidra (кратко)

Плагин **не** использует magnet/торрент ядра: Hubcap/Steam search → lua + keys → LumaCore (Windows) / SLSsteam (Linux) → Steam download, с fallback на DepotDownloaderMod (.NET 9).

```bash
export ARACHNEL_SDK_DIR=~/PetWork/Arachnel
cd ~/PetWork/arachnel-plugin-steamidra
./run.sh   # → ~/.local/share/PetWork/Arachnel/plugins/steamidra/
```

Third-party notices: в репозитории плагина `NOTICE.md` / `LICENSE` (GPL-3).

## Минимальный цикл (FreeTP)

```powershell
git clone …/Arachnel
git clone …/arachnel-plugin-freetp
cd arachnel-plugin-freetp
$env:ARACHNEL_SDK_DIR = "C:\path\to\Arachnel"
.\run.ps1
cd ..\Arachnel
.\run.ps1
```

Плагин окажется в `%LOCALAPPDATA%\PetWork\Arachnel\plugins\freetp\`.  
Либо установите `arachnel-plugin-freetp/build-win/dist/freetp.arach` через **Настройки → Плагины**.

## Контракт

`ISourcePlugin` — `src/core/plugin_interface.h`.  
API: `src/core/plugin_api.h` — **v3** (`owns_download` / plugin jobs); хост всё ещё грузит плагины с **apiVersion 2**.  
См. также [ARCHITECTURE.md](../docs/ARCHITECTURE.md).

Ядро не хранит исходники плагинов; папка `plugins/` здесь — только этот README.
