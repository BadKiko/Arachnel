#!/usr/bin/env python3
"""One-off extractor for hubcap leftover audit — delete after use."""
import re
import sys
from pathlib import Path

raw = Path(sys.argv[1]).read_text(encoding="utf-8", errors="ignore")

print("=== admin-api helper calls ===")
calls = sorted(set(re.findall(r"(?:ja|Ma|Na|Pa|Fa|Ia)\([`'\"](/[^`'\"]+)[`'\"]", raw)))
for h in calls:
    print(h)

print("\n=== /admin* string literals ===")
admins = sorted(set(re.findall(r"[`'\"](/admin[^`'\"]*)[`'\"]", raw)))
for h in admins:
    print(h)

print("\n=== U(`/...`) fetch paths ===")
ufetch = sorted(set(re.findall(r"U\([`'\"](/[^`'\"$]+)[`'\"]", raw)))
for h in ufetch:
    print(h)

print("\n=== SPA-ish path literals ===")
paths = sorted(
    set(
        re.findall(
            r"[`'\"](/(?:admin|auth|download|dashboard|settings|api-keys|depot|docs|debug|metrics|users|requests)[^`'\"]*)[`'\"]",
            raw,
        )
    )
)
for h in paths:
    print(h)

print("\n=== role_level contexts ===")
for m in re.finditer(r".{0,50}role_level.{0,90}", raw):
    s = m.group(0).replace("\n", " ")
    if "DebugValue" in s:
        continue
    print(s[:180])

print("\n=== interesting secrets-ish ===")
for pat in [
    r"SOLUS_[A-Z0-9_]+",
    r"INTERNAL_API_TOKEN",
    r"smm_[a-f0-9]{20,}",
    r"discord\.com/api/webhooks/\d+/[A-Za-z0-9_-]+",
    r"tnkjmec\.com[^`'\"]{0,80}",
    r"MorrenusGames[^`'\"]{0,80}",
]:
    hits = sorted(set(re.findall(pat, raw)))
    for h in hits:
        print(f"{pat}: {h}")

print("\n=== /api/v1 and /api/v2 full-ish ===")
apis = sorted(set(re.findall(r"/api/v[012]/[A-Za-z0-9_./{}?-]+", raw)))
for h in apis:
    print(h)
