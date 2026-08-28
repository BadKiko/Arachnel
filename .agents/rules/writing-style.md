---
description: Natural casual English for README, commits, docs, and any prose
alwaysApply: true
---

# Writing style (prose)

Write like a normal English-speaking developer on GitHub/Discord/Reddit. Not like AI, marketing, or a translator.

## Do

- Simple, conversational sentences
- Contractions forms: it's, I'm, don't, can't, I'll, you're
- Light internet wording when it fits (yeah, kinda, tbh, btw). Don't force slang every line
- Short and direct for GitHub (README, commits, issues)
- Rewrite for how a native speaker would say it. Don't translate word-for-word from Russian
- Skip the trailing period if it feels natural (esp. commit subjects / short README lines)

## Don't

- Em dashes (`—`). Use `-`, commas, periods, or split the sentence
- Corporate / academic / polished filler: "It's worth noting", "In today's world", "Delve", "Leverage", "Utilize", "Furthermore", "Moreover", "As an AI"
- Motivational or dramatic tone unless the source already has it
- Over-explain. Don't invent extra facts
- Emojis unless the original has them
- Product-brochure README voice ("seamless", "powerful", "delightful", Electron/Hydra comparisons nobody asked for)

## Commits

- Subject only unless asked for a body
- Casual one-liner, no `fix:`/`feat:` theater required
- No `Co-authored-by: Cursor`

## Examples

```text
❌ pin QmlMaterial so Setup starts after upstream Layouts split.
   v0.1.31 pulled QmlMaterial main with Qcm.Material.Layouts…

✅ pin qmlmaterial so setup doesn't crash on launch
```

```text
❌ Arachnel is a Material 3 desktop launcher that empowers users to…
✅ small game launcher for windows/linux. install a plugin, pick a game, hit play
```
