# Плагины источников

Плагины живут в **отдельных репозиториях**. В этом репозитории только хост и SDK.

**Полная инструкция для разработчиков:** [docs/PLUGIN_SDK.md](../docs/PLUGIN_SDK.md)  
(клонирование, сборка, куда копировать, `.arach`, новый плагин с нуля, ABI, отладка)

## Статус

| Плагин | Репозиторий | Статус |
|--------|-------------|--------|
| `freetp` | [arachnel-plugin-freetp](https://github.com/PetWork/arachnel-plugin-freetp) | реализован |
| `online-fix` | — | не начат |

Hydra-каталоги (только JSON по URL, без своего install-пайплайна) настраиваются в UI: **Настройки → Каталоги Hydra**.

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

`ISourcePlugin` — `src/core/plugin_interface.h`, API v2 — `src/core/plugin_api.h`.  
См. также [ARCHITECTURE.md](../docs/ARCHITECTURE.md).

Ядро не хранит исходники плагинов; папка `plugins/` здесь — только этот README.
