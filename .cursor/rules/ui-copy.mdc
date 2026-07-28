---
description: User-facing UI copy - concise, no prompt tone, hyphen not em dash
alwaysApply: true
---

# UI copy (Arachnel)

English source in `qsTr` / `translate`; Russian in `translations/arachnel_ru.ts` (see `i18n.mdc`).

## Tone

- Write for the end user, not as a restatement of the agent's task or a product brief.
- One short sentence when a description is needed. Prefer none if the screen is self-explanatory.
- Do not narrate implementation details (folders, "every package shows…", "for transparency", "public repository", etc.) unless the user must act on them.
- Do not paste or paraphrase chat prompts into UI strings.

## Dashes

- In user-visible strings use ASCII hyphen-minus `-` (e.g. `v1 - not loaded`, `1-5 GB`).
- Do not use em dash `—` or en dash `–` in UI copy.
- Code comments may keep whatever punctuation; this rule is for `qsTr` / `translate` / Messages and their `.ts` translations.

## Examples

```
// BAD
qsTr("Official plugins from the Arachnel catalog. Install adds them to your plugins folder. Every package shows its source repository and download URL.")

// GOOD
qsTr("Install official plugins.")
```

```
// BAD
qsTr("Plugins add catalogs and handle install, updates, and launch. Each plugin shows its public source repository and catalog URL.")

// GOOD
qsTr("Plugins provide catalogs, install, updates, and launch.")
```

```
// BAD
qsTr("No URL — catalog will not load")

// GOOD
qsTr("No URL - catalog will not load")
```
