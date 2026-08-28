---
description: No prompt-narration or chat-echo comments in code
alwaysApply: true
---

# Comments

Write code for humans reading the repo, not a transcript of the chat.

## Do

- Comment only when the **why** is non-obvious (invariant, bug trap, platform quirk)
- Keep it short - one line when possible
- Prefer clearer names / structure over a comment

## Don't

- Narrate the change or the prompt ("after RAM pass…", "Jump-scroll used to…", "55k Steam", "don't parse again")
- Restate what the next line obviously does
- Essay comments that belong in a commit message or PR
- UI/`qsTr` strings that sound like agent notes (see `ui-copy.mdc`)

## Examples

```cpp
// BAD - chat residue
// Jump-scroll used to always drop waiters here. Then an in-flight download finished
// with an empty waiter set… blank posters until hover.

// OK - non-obvious invariant
// Keep in-flight waiters so ready still applies after cancel.
```

```cpp
// BAD
// Bulk prepare: ints/ids only. Never attach description/screenshots to 55k rows.

// OK (or omit if prepareCatalogEntry name is enough)
// List path: ids/scores only; cold fields on enrich.
```

When editing a file, delete prompt-style comments you see nearby - don't leave them to "document the session".
