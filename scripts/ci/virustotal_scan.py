#!/usr/bin/env python3
"""Upload release binaries to VirusTotal and write a shields.io endpoint badge.

Free public API: keep request_rate low (default 4/min). Large files (>32 MiB)
use the upload_url flow (2 API calls per file).
"""

from __future__ import annotations

import hashlib
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any


VT_API = "https://www.virustotal.com/api/v3"
RATE_SLEEP = 16.0  # ~4 req/min for free keys
LARGE_FILE = 32 * 1024 * 1024


class VtError(RuntimeError):
    pass


def api_key() -> str:
    key = os.environ.get("VT_API_KEY", "").strip()
    if not key:
        raise VtError("VT_API_KEY is not set")
    return key


def request(
    method: str,
    path: str,
    *,
    data: bytes | None = None,
    headers: dict[str, str] | None = None,
    raw_url: str | None = None,
) -> dict[str, Any]:
    url = raw_url or f"{VT_API}{path}"
    hdrs = {"x-apikey": api_key(), "Accept": "application/json"}
    if headers:
        hdrs.update(headers)
    req = urllib.request.Request(url, data=data, headers=hdrs, method=method)
    try:
        with urllib.request.urlopen(req, timeout=300) as resp:
            body = resp.read()
            if not body:
                return {}
            return json.loads(body.decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise VtError(f"VT {method} {url} -> HTTP {exc.code}: {detail}") from exc


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def get_file_report(file_hash: str) -> dict[str, Any] | None:
    try:
        return request("GET", f"/files/{file_hash}")
    except VtError as exc:
        if "HTTP 404" in str(exc):
            return None
        raise


def upload_file(path: Path) -> str:
    """Return analysis id."""
    size = path.stat().st_size
    if size >= LARGE_FILE:
        upload_url = request("GET", "/files/upload_url").get("data")
        if not isinstance(upload_url, str) or not upload_url:
            raise VtError("Missing upload_url from VirusTotal")
        time.sleep(RATE_SLEEP)
        boundary = f"----arachnel{os.getpid()}{int(time.time())}"
        filename = path.name
        head = (
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
            f"Content-Type: application/octet-stream\r\n\r\n"
        ).encode("utf-8")
        tail = f"\r\n--{boundary}--\r\n".encode("utf-8")
        payload = head + path.read_bytes() + tail
        result = request(
            "POST",
            "",
            data=payload,
            headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
            raw_url=upload_url,
        )
    else:
        boundary = f"----arachnel{os.getpid()}{int(time.time())}"
        filename = path.name
        head = (
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
            f"Content-Type: application/octet-stream\r\n\r\n"
        ).encode("utf-8")
        tail = f"\r\n--{boundary}--\r\n".encode("utf-8")
        payload = head + path.read_bytes() + tail
        result = request(
            "POST",
            "/files",
            data=payload,
            headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
        )
    analysis_id = result.get("data", {}).get("id")
    if not analysis_id:
        raise VtError(f"Upload did not return analysis id for {path.name}: {result}")
    return str(analysis_id)


def wait_analysis(analysis_id: str, timeout_s: int = 600) -> dict[str, Any]:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        time.sleep(RATE_SLEEP)
        data = request("GET", f"/analyses/{analysis_id}")
        status = data.get("data", {}).get("attributes", {}).get("status")
        print(f"analysis {analysis_id[:24]}… status={status}", flush=True)
        if status == "completed":
            return data
        if status in {"failed", "error"}:
            raise VtError(f"Analysis failed: {data}")
    raise VtError(f"Timed out waiting for analysis {analysis_id}")


def stats_from_report(report: dict[str, Any]) -> dict[str, int]:
    attrs = report.get("data", {}).get("attributes", {})
    stats = attrs.get("last_analysis_stats") or {}
    return {
        "malicious": int(stats.get("malicious") or 0),
        "suspicious": int(stats.get("suspicious") or 0),
        "undetected": int(stats.get("undetected") or 0),
        "harmless": int(stats.get("harmless") or 0),
        "timeout": int(stats.get("timeout") or 0),
        "failure": int(stats.get("failure") or 0),
        "type-unsupported": int(stats.get("type-unsupported") or 0),
    }


def ensure_scanned(path: Path) -> tuple[str, dict[str, int], str]:
    digest = sha256_file(path)
    print(f"==> {path.name} sha256={digest}", flush=True)
    report = get_file_report(digest)
    time.sleep(RATE_SLEEP)
    if report is None:
        print(f"uploading {path.name} ({path.stat().st_size} bytes)", flush=True)
        analysis_id = upload_file(path)
        wait_analysis(analysis_id)
        time.sleep(RATE_SLEEP)
        report = get_file_report(digest)
        if report is None:
            raise VtError(f"No report after upload for {path.name}")
    else:
        # Refresh engines against this hash (cheap when already known).
        print(f"requesting rescan for {path.name}", flush=True)
        try:
            rescan = request("POST", f"/files/{digest}/analyse")
            analysis_id = rescan.get("data", {}).get("id")
            if analysis_id:
                wait_analysis(str(analysis_id))
                time.sleep(RATE_SLEEP)
                refreshed = get_file_report(digest)
                if refreshed:
                    report = refreshed
        except VtError as exc:
            print(f"rescan skipped: {exc}", flush=True)

    stats = stats_from_report(report)
    link = f"https://www.virustotal.com/gui/file/{digest}"
    return digest, stats, link


def write_badge(path: Path, results: list[dict[str, Any]]) -> None:
    total_mal = sum(r["stats"]["malicious"] for r in results)
    total_sus = sum(r["stats"]["suspicious"] for r in results)
    parts = []
    for r in results:
        label = "Win" if r["name"].lower().endswith(".exe") else (
            "Linux" if "appimage" in r["name"].lower() else r["name"]
        )
        s = r["stats"]
        parts.append(f"{label} {s['malicious']}/{s['malicious'] + s['suspicious'] + s['undetected'] + s['harmless']}")

    if total_mal == 0 and total_sus == 0:
        message = "clean · " + " · ".join(parts) if parts else "clean"
        color = "brightgreen"
    elif total_mal == 0:
        message = "ok · " + " · ".join(parts)
        color = "yellow"
    else:
        message = "flags · " + " · ".join(parts)
        color = "red"

    badge = {
        "schemaVersion": 1,
        "label": "VirusTotal",
        "message": message,
        "color": color,
        "namedLogo": "virustotal",
        "cacheSeconds": 3600,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(badge, indent=2) + "\n", encoding="utf-8")

    summary = {
        "updated": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "files": results,
        "totals": {"malicious": total_mal, "suspicious": total_sus},
    }
    summary_path = path.with_name("virustotal-latest.json")
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {path}", flush=True)
    print(f"wrote {summary_path}", flush=True)


def append_release_body(tag: str, results: list[dict[str, Any]]) -> None:
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    repo = os.environ.get("GITHUB_REPOSITORY", "").strip()
    if not token or not repo or not tag:
        print("skip release body update (missing token/repo/tag)", flush=True)
        return

    api = f"https://api.github.com/repos/{repo}/releases/tags/{urllib.parse.quote(tag)}"
    req = urllib.request.Request(
        api,
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with urllib.request.urlopen(req, timeout=60) as resp:
        release = json.loads(resp.read().decode("utf-8"))

    body = release.get("body") or ""
    marker = "<!-- virustotal-scan -->"
    section_lines = [
        "",
        marker,
        "## VirusTotal",
        "",
    ]
    for r in results:
        s = r["stats"]
        section_lines.append(
            f"- [{r['name']}]({r['link']}): "
            f"{s['malicious']} malicious, {s['suspicious']} suspicious, "
            f"{s['undetected'] + s['harmless']} clean"
        )
    section_lines.append("")
    section = "\n".join(section_lines)

    if marker in body:
        before, _, rest = body.partition(marker)
        # Drop previous VT section until next ## or end
        rest_lines = rest.splitlines()
        # first line after marker was old heading; skip until blank+## or end
        i = 0
        while i < len(rest_lines) and not (
            rest_lines[i].startswith("## ") and "VirusTotal" not in rest_lines[i]
        ):
            i += 1
            if i > 40:
                break
        after = "\n".join(rest_lines[i:]).lstrip("\n")
        body = before.rstrip() + section + (("\n" + after) if after else "\n")
    else:
        body = body.rstrip() + section

    payload = json.dumps({"body": body}).encode("utf-8")
    patch = urllib.request.Request(
        release["url"],
        data=payload,
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": "application/json",
        },
        method="PATCH",
    )
    with urllib.request.urlopen(patch, timeout=60):
        pass
    print(f"updated release body for {tag}", flush=True)


def collect_files() -> list[Path]:
    raw = os.environ.get("VT_FILES", "").strip()
    files: list[Path] = []
    if raw:
        for line in raw.splitlines():
            line = line.strip()
            if line:
                files.append(Path(line))
    else:
        root = Path(os.environ.get("VT_SCAN_DIR", "release"))
        if root.is_dir():
            for p in sorted(root.iterdir()):
                if p.suffix.lower() in {".exe", ".appimage"} or p.name.endswith(".AppImage"):
                    files.append(p)
    existing = [p for p in files if p.is_file()]
    if not existing:
        raise VtError(f"No files to scan (VT_FILES / VT_SCAN_DIR). got={files}")
    return existing


def main() -> int:
    files = collect_files()
    results: list[dict[str, Any]] = []
    for path in files:
        digest, stats, link = ensure_scanned(path)
        results.append(
            {
                "name": path.name,
                "sha256": digest,
                "stats": stats,
                "link": link,
            }
        )
        print(f"{path.name}: {stats} -> {link}", flush=True)
        time.sleep(RATE_SLEEP)

    badge_path = Path(os.environ.get("VT_BADGE_PATH", "docs/badges/virustotal.json"))
    write_badge(badge_path, results)

    tag = os.environ.get("VT_RELEASE_TAG", "").strip()
    if tag:
        append_release_body(tag, results)

    # Soft fail only on clear multi-engine hits (unsigned installers get FPs).
    max_mal = max((r["stats"]["malicious"] for r in results), default=0)
    fail_at = int(os.environ.get("VT_FAIL_MALICIOUS_AT", "5"))
    if max_mal >= fail_at:
        print(f"::error::VirusTotal malicious count {max_mal} >= {fail_at}", flush=True)
        return 1
    if max_mal > 0:
        print(
            f"::warning::VirusTotal reported {max_mal} malicious engine(s). "
            "Common for unsigned builds; check the GUI links.",
            flush=True,
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except VtError as exc:
        print(f"::error::{exc}", flush=True)
        raise SystemExit(1) from exc
