# Translating Arachnel

Arachnel uses **Qt Linguist** (`.ts` files) with **English** as the source language.

## Files

| File | Purpose |
|------|---------|
| `translations/arachnel_en.ts` | **Source template** (English strings only, monolingual) |
| `translations/arachnel_ru.ts` | Russian translations (bilingual: source + translation) |
| `translations/generated/arachnel_ids.ts` | **Committed** id-based catalog for `qsTrId()` (see below) |
| `qml/i18n/Messages.qml` | Long UI copy with **short `qsTrId` keys** for Weblate |
| `qml/**/*.qml` | Short UI strings via `qsTr("...")` |
| `src/core/*.cpp` | C++ strings via `QCoreApplication::translate("Core", ...)` |

## Weblate

Project: [hosted.weblate.org/projects/arachnel](https://hosted.weblate.org/projects/arachnel/)

Repo includes `weblate.yml` with recommended component settings:

- **Template / monolingual base:** `translations/arachnel_en.ts`
- **Translation files:** `translations/arachnel_*.ts` (e.g. `arachnel_ru.ts`)
- **Source language:** English — the `en` file is **not** a language to translate; it is the string catalog.

If English shows **0%** on Weblate, the component is misconfigured: enable **monolingual base language file** = `arachnel_en.ts`, or remove English from target languages.

### Short keys (Weblate)

Long help texts use `qsTrId("help.catalog_intro")` in `qml/i18n/Messages.qml`.  
Weblate shows the **ID** (`help.catalog_intro`) instead of a giant `ContextName+Full English sentence` key.

Short labels still use `qsTr()` with context = QML file name (normal for Qt).

`arachnel_ids.ts` is generated from `arachnel_en.ts` (messages with `id="..."`) and **committed to git** so the app build does not need Python. After changing id-based strings, regenerate:

```bash
python tools/prepare_id_translations.py translations/generated/arachnel_ids.ts
```

## Update strings after code changes

```bash
cmake --build build --target update_translations
python3 tools/normalize_translations.py
cmake --build build --target release_translations
```

Or:

```bash
cmake --build build --target arachnel_normalize_translations
```

Commit updated `translations/*.ts` files.

## Add a new language

1. Copy `translations/arachnel_ru.ts` to `translations/arachnel_<lang>.ts`
2. Change `language="..."` in the XML header
3. Add the file to `cmake/ArachnelTranslations.cmake`
4. Reconfigure CMake, run `update_translations` + `normalize_translations.py` + `release_translations`
5. Add the language to `languageOptions` in `qml/settings/SettingsAppearancePage.qml`

## Translate without Weblate

1. Install [Qt Linguist](https://doc.qt.io/qt-6/linguist-translators.html)
2. Open `translations/arachnel_ru.ts`
3. Fill in translations (English source is in `<source>`, write target in `<translation>`)
4. Send a pull request

## Runtime

- Default UI language: **English** — loads `arachnel_ids.qm` so `qsTrId()` resolves (no inline English fallback)
- Other languages load `arachnel_<lang>.qm` (`qsTr` / `translate`) **and** `arachnel_ids_<lang>.qm` (`qsTrId`); missing id catalog falls back to English ids
- Users change language in **Settings → Appearance → Language**
- Choice is stored in `settings.json` as `uiLanguage`
