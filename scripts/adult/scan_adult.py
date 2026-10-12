#!/usr/bin/env python3
"""Mark Steam games with nudity / sexual content in an Arachnel catalog.

Neither the catalog feeds nor Steam's genre list say which games are 18+, so this asks the store:
IStoreBrowseService/GetItems returns `content_descriptorids` (1 some nudity or sexual content,
3 adult only sexual content, 4 frequent nudity or sexual content) for 200 apps per request.

* by default any of them counts ("any nudity"); --strict keeps only 3 and 4 (explicit content);
* demos listed under an adult game are adult too;
* soundtracks / DLC (`parent_appid`) follow their parent game;
* apps the store does not list at all (removed, never public) count as adult when the title has a
  clear weak marker ("sex", "incest", ...), because there is nothing else to go on.

Answers are cached (--cache), so a run only asks about new apps and re-checks old answers every
--recheck-days. Run it after the catalog is built, e.g. from cron on the server that serves it:

    scan_adult.py --catalog catalog.json --cache adult-cache.json --out adult-ids.json

Serve adult-ids.json at /v1/adult-ids next to the catalog: Arachnel downloads it (with ETag) and
hides those games when the user chose to hide adult content. --annotate additionally writes
"adult": true into every flagged catalog entry.
Only the standard library is used.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

API = "https://api.steampowered.com/IStoreBrowseService/GetItems/v1"
BATCH = 200  # the endpoint is a GET; Steam rejects URLs over ~8 KB
CACHE_VERSION = 2
WEAK = re.compile(r"(?<![a-z0-9])(sex|sexy|sexual|incest|slut|sluts|whore|horny|orgy)(?![a-z0-9])", re.I)


def fetch_batch(app_ids: list[int], retries: int = 5) -> list[dict]:
    payload = {
        "ids": [{"appid": a} for a in app_ids],
        "context": {"language": "english", "country_code": "US"},
        "data_request": {},
    }
    url = API + "?input_json=" + urllib.parse.quote(json.dumps(payload, separators=(",", ":")))
    delay = 2.0
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "Arachnel-adult-scan/1"})
            with urllib.request.urlopen(req, timeout=60) as resp:
                data = json.loads(resp.read())
            return data.get("response", {}).get("store_items", [])
        except urllib.error.HTTPError as err:
            if err.code not in (429, 500, 502, 503, 504):
                raise
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
            pass
        time.sleep(delay)
        delay = min(delay * 2, 60)
    raise RuntimeError(f"batch of {len(app_ids)} apps failed after {retries} attempts")


def load_cache(path: Path | None) -> dict:
    empty = {"version": CACHE_VERSION, "apps": {}}
    if not path or not path.exists():
        return empty
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return empty
    return data if data.get("version") == CACHE_VERSION and isinstance(data.get("apps"), dict) else empty


def atomic_write(path: Path, text: str) -> None:
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=path.name + ".")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as fh:
            fh.write(text)
        os.replace(tmp, path)
    except BaseException:
        os.unlink(tmp)
        raise


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--catalog", type=Path, required=True, help="catalog JSON with an entries[] array")
    ap.add_argument("--cache", type=Path, default=None, help="answers cache (read and written)")
    ap.add_argument("--annotate", type=Path, default=None, help="write the catalog back with adult:true")
    ap.add_argument("--out", type=Path, default=None, help="write {appIds:[...]} here")
    ap.add_argument("--strict", action="store_true", help="only explicit content (descriptors 3 and 4)")
    ap.add_argument("--workers", type=int, default=4)
    ap.add_argument("--recheck-days", type=int, default=60)
    ap.add_argument("--max-apps", type=int, default=0, help="debug: only look at the first N apps")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    raw = json.loads(args.catalog.read_text(encoding="utf-8"))
    entries = raw["entries"] if isinstance(raw, dict) else raw
    titles: dict[int, str] = {}
    for entry in entries:
        app = str(entry.get("steamAppId") or "")
        if not app and str(entry.get("id", "")).startswith("steam-"):
            app = str(entry["id"])[6:]
        if app.isdigit():
            titles[int(app)] = str(entry.get("title") or "")
    app_ids = sorted(titles)
    if args.max_apps:
        app_ids = app_ids[: args.max_apps]

    min_level = 2 if args.strict else 1
    cache = load_cache(args.cache)
    apps: dict[str, dict] = cache["apps"]  # appid -> {"a": adult, "u": unlisted, "p": parent, "t": checked at}
    now = int(time.time())
    stale_before = now - args.recheck_days * 86400
    todo = [a for a in app_ids if str(a) not in apps or apps[str(a)].get("t", 0) < stale_before]
    if args.verbose:
        print(f"{len(app_ids)} apps, {len(todo)} to ask about", file=sys.stderr)

    def record(item: dict, asked: int) -> None:
        if item.get("success", 1) != 1:
            apps[str(asked)] = {"a": 0, "u": 1, "t": now}
            return
        app = int(item.get("appid") or asked)
        related = item.get("related_items") or {}
        descriptors = set(item.get("content_descriptorids") or [])
        # level: 2 explicit (3, 4), 1 some nudity (1), 0 none
        level = 2 if descriptors & {3, 4} else (1 if 1 in descriptors else 0)
        adult = level >= min_level
        row = {"a": level, "u": 0, "t": now}
        parent = related.get("parent_appid")
        if parent:
            row["p"] = int(parent)
        apps[str(app)] = row
        if adult:
            for demo in related.get("demos") or []:
                demo_id = int(demo.get("appid") or 0)
                if demo_id:
                    apps[str(demo_id)] = {"a": level, "u": 0, "t": now}

    def run(queue: list[int]) -> None:
        batches = [queue[i : i + BATCH] for i in range(0, len(queue), BATCH)]
        done = 0
        with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
            futures = {pool.submit(fetch_batch, b): b for b in batches}
            for fut in as_completed(futures):
                batch = futures[fut]
                items = fut.result()
                answered = set()
                for item in items:
                    asked = int(item.get("id") or item.get("appid") or 0)
                    if asked:
                        record(item, asked)
                        answered.add(asked)
                for a in batch:  # the API sometimes omits an id altogether
                    if a not in answered and str(a) not in apps:
                        apps[str(a)] = {"a": 0, "u": 1, "t": now}
                done += 1
                if args.verbose and done % 25 == 0:
                    print(f"  {done}/{len(batches)} batches", file=sys.stderr)
                if args.cache and done % 100 == 0:
                    atomic_write(args.cache, json.dumps(cache, separators=(",", ":")))

    run(todo)

    # Soundtracks / DLC follow their parent: ask about parents we have not seen.
    parents = sorted({row["p"] for row in apps.values() if row.get("p") and str(row["p"]) not in apps})
    if parents:
        if args.verbose:
            print(f"{len(parents)} parent apps to ask about", file=sys.stderr)
        run(parents)

    adult_ids: set[int] = set()
    for key, row in apps.items():
        app = int(key)
        if row.get("a", 0) >= min_level:
            adult_ids.add(app)
        elif row.get("p") and (apps.get(str(row["p"])) or {}).get("a", 0) >= min_level:
            adult_ids.add(app)
        elif row.get("u") and WEAK.search(titles.get(app, "")):
            adult_ids.add(app)
    adult_ids &= set(titles)

    if args.cache:
        args.cache.parent.mkdir(parents=True, exist_ok=True)
        atomic_write(args.cache, json.dumps(cache, separators=(",", ":")))
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        atomic_write(
            args.out,
            json.dumps({"version": 1, "generatedAt": now, "count": len(adult_ids), "appIds": sorted(adult_ids)},
                       separators=(",", ":")),
        )
    if args.annotate:
        flagged = 0
        for entry in entries:
            app = str(entry.get("steamAppId") or "")
            if not app and str(entry.get("id", "")).startswith("steam-"):
                app = str(entry["id"])[6:]
            if app.isdigit() and int(app) in adult_ids:
                entry["adult"] = True
                flagged += 1
            else:
                entry.pop("adult", None)
        atomic_write(args.annotate, json.dumps(raw, ensure_ascii=False, separators=(",", ":")))
        print(f"annotated {flagged} entries", file=sys.stderr)
    print(f"{len(adult_ids)} adult apps out of {len(titles)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
