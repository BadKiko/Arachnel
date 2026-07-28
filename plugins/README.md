# Плагины источников

Плагины живут в **отдельных репозиториях**. В этом репозитории только хост и SDK.
Исходники каждого плагина открыты — ссылки ниже и в UI (**Настройки → Магазин плагинов** / **Плагины** / **Источники**).

**Полная инструкция для разработчиков:** [docs/PLUGIN_SDK.md](../docs/PLUGIN_SDK.md)

## Статус

| Плагин | Репозиторий | Статус |
|--------|-------------|--------|
| `freetp` | [arachnel-plugin-freetp](https://github.com/PetWork/arachnel-plugin-freetp) | реализован (API v4, torrent → install) |
| `steamidra` | [arachnel-plugin-steamidra](https://gitlab.com/BadKiko/arachnel-plugin-steamidra) | реализован (API v4, `owns_download`, GPL-3) |
| `online-fix` | — | не начат |

Официальный список пакетов (что ставит лаунчер из магазина):

`https://gitlab.com/BadKiko/arachnel-plugins-sourcelist/-/raw/main/plugins.json`

Schema **v2**: `builds[]` with `minArachnel` / `maxArachnel` / `abiToken`. The launcher picks a compatible build for the running app version.

В UI для каждого пакета показываются **URL исходников** и **URL скачиваемого `.arach`**.  
Для установленных плагинов — ещё **URL каталога игр** (откуда лаунчер грузит список тайтлов).

Hydra-каталоги (только JSON по URL, без своего install-пайплайна) настраиваются в UI: **Настройки → Источники**.

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

Плагин окажется в `%APPDATA%\Arachnel\plugins\freetp\`.  
Либо установите `arachnel-plugin-freetp/build-win/dist/freetp.arach` через **Настройки → Плагины**.

## Контракт

`ISourcePlugin` — `src/core/plugins/plugin_interface.h`.  
API: `src/core/plugins/plugin_api.h` — **v4** (JSON catalog across the DLL; host still loads API **2..3**).  
Build plugins with the **same MinGW + Qt kit** as Arachnel `release.yml`; set `ARACHNEL_SDK_REF` to that release tag.  
См. также [ARCHITECTURE.md](../docs/ARCHITECTURE.md).

Ядро не хранит исходники плагинов; папка `plugins/` здесь — только этот README.
