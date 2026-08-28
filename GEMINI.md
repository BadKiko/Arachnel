# Arachnel Workspace Guidelines

## Core Principles

- **Cross-Platform:** Windows and Linux support. On Linux, Windows games run via Proton. Always design install, online-fix, and launch pipelines for both platforms.
- **Plugin Architecture:** Sources (Steam, FreeTP, Online-Fix, etc.) are isolated plugins implementing `ISourcePlugin`. Never break plugin binary interface (ABI): new virtual methods must strictly be appended at the end of `ISourcePlugin`.
- **Material 3 UI:** Desktop UI built with Qt 6 QML and Material 3 design system (`Qcm.Material`).

---

## Localization (i18n)

- **Source Strings:** All user-facing strings in code must be in English wrapped with `qsTr("...")` in QML or `QCoreApplication::translate("Core", "...")` in C++.
- **Translations:** Russian translations are maintained in `translations/arachnel_ru.ts`.
- **Workflow:** Whenever adding or changing UI strings:
  1. Run `lupdate` (`translations/arachnel_en.ts`, `translations/arachnel_ru.ts`).
  2. Translate all new strings in `arachnel_ru.ts` (never leave `type="unfinished"`).
  3. Compile `.qm` files with `lrelease`.
- **No hardcoded Russian in source code.** No `qsTrId` or standalone dictionary files.

---

## UI Copy & Punctuation

- **Tone:** Concise, user-focused, plain language. No prompt-narration or AI jargon in user-facing copy.
- **Dashes:** Use ASCII hyphen-minus `-` in UI strings. Do NOT use em dash `—` or en dash `–`.

---

## Code Quality & Comments

- **Comments:** Comment only non-obvious invariants, platform quirks, or bug traps. Keep comments to one line when possible.
- **No Prompt Narration:** Never write comments narrating the chat session, previous bugs, or prompt instructions.
- **Writing Style:** Natural, concise English for commits and documentation.

---

## Rules & Skills

- Detailed rules reside in `.agents/rules/`.
- Specialized skills reside in `.agents/skills/`.
