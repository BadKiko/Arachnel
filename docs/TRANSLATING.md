# Translating Arachnel

Arachnel uses **Qt Linguist** (`.ts` → `.qm`) with **English** as the source language in code.

## Files

| File | Purpose |
|------|---------|
| `translations/arachnel_en.ts` | Source catalog (English `<source>` strings for Weblate / lupdate) |
| `translations/arachnel_ru.ts` | Russian translations |
| `qml/**/*.qml` | `qsTr("...")` |
| `qml/i18n/Messages.qml` | Shared / long UI copy via `qsTr("...")` |
| `src/core/*.cpp` | `QCoreApplication::translate("Core", ...)` |

English UI needs no translator: Qt falls back to the string in source. Other languages load `:/i18n/arachnel_<lang>.qm`.

## Weblate

Project: [hosted.weblate.org/projects/arachnel](https://hosted.weblate.org/projects/arachnel/)

See `weblate.yml`: monolingual base = `translations/arachnel_en.ts`, targets = `translations/arachnel_*.ts`.

## Update strings after code changes

```bash
cmake --build build --target update_translations
# Edit translations/arachnel_ru.ts in Qt Linguist (or by hand).
# Clear type="unfinished" once translated.
cmake --build build --target release_translations
```

Commit updated `translations/*.ts`.

## Add a new language

1. Copy `translations/arachnel_ru.ts` to `translations/arachnel_<lang>.ts`
2. Change `language="..."` in the XML header
3. Add the file to `cmake/ArachnelTranslations.cmake`
4. Reconfigure CMake, run `update_translations` + `release_translations`
5. Add the language to `languageOptions` in `qml/settings/SettingsAppearancePage.qml`

## Translate without Weblate

1. Install [Qt Linguist](https://doc.qt.io/qt-6/linguist-translators.html)
2. Open `translations/arachnel_ru.ts`
3. Fill `<translation>` for each English `<source>`
4. Send a pull request

## Runtime

- Default: **English** (no `.qm` loaded — strings from code)
- Other languages: `arachnel_<lang>.qm` from resources
- Settings → Appearance → Language → `settings.json` `uiLanguage`
