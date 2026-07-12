# Translating Arachnel

Arachnel uses **Qt Linguist** (`.ts` files) with **English** as the source language.

## Files

| File | Purpose |
|------|---------|
| `translations/arachnel_en.ts` | Source catalog (English strings from code) |
| `translations/arachnel_ru.ts` | Russian translations |
| `qml/**/*.qml` | UI strings via `qsTr("...")` |

## Update strings after code changes

```bash
cmake --build build-win --target update_translations
cmake --build build-win --target release_translations
```

Commit updated `translations/*.ts` files.

## Add a new language

1. Copy `translations/arachnel_ru.ts` to `translations/arachnel_<lang>.ts`
2. Change `language="..."` in the XML header
3. Add the file to `cmake/ArachnelTranslations.cmake`
4. Reconfigure CMake and run `release_translations`
5. Add the language to `languageOptions` in `qml/settings/SettingsAppearancePage.qml`

## Weblate (recommended for contributors)

1. Register the project on [hosted.weblate.org](https://hosted.weblate.org/) (free for open source)
2. Repository: `https://github.com/BadKiko/Arachnel`
3. Component settings:
   - **File format:** Qt Linguist `.ts`
   - **File mask:** `translations/*.ts`
   - **Source language:** English

Contributors translate in the browser; Weblate opens pull requests with updated `.ts` files.

## Translate without Weblate

1. Install [Qt Linguist](https://doc.qt.io/qt-6/linguist-translators.html)
2. Open `translations/arachnel_ru.ts` (or your language)
3. Fill in translations
4. Send a pull request

## Runtime

- Default UI language: **English**
- Users change language in **Settings → Appearance → Language**
- Choice is stored in `settings.json` as `uiLanguage`
