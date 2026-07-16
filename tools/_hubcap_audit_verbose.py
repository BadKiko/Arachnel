#!/usr/bin/env python3
"""Verbose errors, headers, leftover static, CORS — no API key usage."""
from __future__ import annotations

import json
import ssl
import urllib.error
import urllib.request
from pathlib import Path

BASE = "https://hubcapmanifest.com"
ctx = ssl.create_default_context()


def req(path: str, method="GET", data=None, headers=None, timeout=20):
    h = {"User-Agent": "HubcapLeftoverAudit/1.0"}
    if headers:
        h.update(headers)
    r = urllib.request.Request(BASE + path, data=data, headers=h, method=method)
    try:
        with urllib.request.urlopen(r, context=ctx, timeout=timeout) as resp:
            body = resp.read(3000)
            return {
                "path": f"{method} {path}",
                "status": resp.status,
                "headers": dict(resp.headers.items()),
                "body": body.decode("utf-8", errors="replace"),
            }
    except urllib.error.HTTPError as e:
        body = e.read(3000)
        return {
            "path": f"{method} {path}",
            "status": e.code,
            "headers": dict(e.headers.items()),
            "body": body.decode("utf-8", errors="replace"),
        }
    except Exception as ex:
        return {"path": f"{method} {path}", "status": "ERR", "headers": {}, "body": str(ex)}


out = []

# Verbose / injection-ish error probing (free)
cases = [
    ("GET", "/api/v1/generate/branches/notanumber", None, None),
    ("GET", "/api/v1/generate/branches/-1", None, None),
    ("GET", "/api/v1/generate/branches/999999999999999999999999", None, None),
    ("GET", "/api/v1/status/abc", None, None),
    ("GET", "/api/v1/health/%2e%2e", None, None),
    ("GET", "/auth/role-name/not-a-snowflake", None, None),
    ("GET", "/auth/role-name/0", None, None),
    ("GET", "/billing/tiers", None, None),
    ("OPTIONS", "/api/v1/health", None, {"Origin": "https://evil.example", "Access-Control-Request-Method": "GET"}),
    ("GET", "/api/v1/health", None, {"Origin": "https://evil.example"}),
    ("GET", "/static/css/style.css", None, None),
    ("GET", "/favicon.ico", None, None),
    ("GET", "/manifest.json", None, None),
    ("GET", "/config.json", None, None),
    ("GET", "/.well-known/security.txt", None, None),
    ("GET", "/robots.txt", None, None),
    ("GET", "/api/v2/client/latest?channel=nightly", None, None),
    ("GET", "/api/v2/app/latest?channel=stable", None, None),
    ("GET", "/api/v2/client/web-prepare?channel=stable", None, None),
    ("GET", "/api/v1/generate/branches/570", None, None),  # Dota
    ("POST", "/api/v1/health", b"{}", {"Content-Type": "application/json"}),
    ("GET", "/admin-api/lua/test", None, {"Accept": "application/json"}),
    ("GET", "/cleanfiles-api/run", None, {"Accept": "application/json"}),
    # FastAPI docs common alternate mounts
    ("GET", "/api/v1/docs", None, None),
    ("GET", "/api/v1/redoc", None, None),
    ("GET", "/api/v1/openapi.json", None, None),
    ("GET", "/openapi", None, None),
    ("GET", "/docs/oauth2-redirect", None, None),
]

for method, path, data, headers in cases:
    r = req(path, method=method, data=data, headers=headers)
    # Keep interesting header subset
    interesting_h = {
        k: v
        for k, v in r["headers"].items()
        if k.lower()
        in {
            "content-type",
            "x-client-ip",
            "access-control-allow-origin",
            "access-control-allow-credentials",
            "access-control-allow-headers",
            "access-control-allow-methods",
            "www-authenticate",
            "server",
            "via",
            "x-powered-by",
            "x-request-id",
            "x-frame-options",
            "content-security-policy",
            "set-cookie",
            "allow",
        }
    }
    out.append(
        {
            "path": r["path"],
            "status": r["status"],
            "headers": interesting_h,
            "snip": r["body"][:500].replace("\n", " "),
        }
    )

# Extract from JS: admin surface inventory + discord + env error strings
js = Path(r"C:\Users\kiril\AppData\Local\Temp\hubcap_index.js").read_text(
    encoding="utf-8", errors="ignore"
)
import re

admin_pages = sorted(set(re.findall(r"[`'\"](/admin/[^`'\"]+)[`'\"]", js)))
discord_links = sorted(set(re.findall(r"https://discord\.com/[^`'\"]+", js)))
env_mentions = sorted(set(re.findall(r"[A-Z][A-Z0-9_]{4,}(?:_TOKEN|_KEY|_SECRET|_URL|_HOST)", js)))
cdn = sorted(set(re.findall(r"https?://[^`'\"]*tnkjmec[^`'\"]*", js)))
# Check for hardcoded private IPs / hostnames
hosts = sorted(
    set(
        re.findall(
            r"(?:localhost|127\.0\.0\.1|10\.\d+\.\d+\.\d+|192\.168\.\d+\.\d+|172\.(?:1[6-9]|2\d|3[0-1])\.\d+\.\d+|[\w-]+\.internal)[^`'\"]{0,40}",
            js,
        )
    )
)

summary = {
    "probes": out,
    "admin_pages": admin_pages,
    "discord_links": discord_links,
    "env_like": [e for e in env_mentions if not e.startswith(("INTERNALS", "DEBUG"))],
    "cdn": cdn,
    "hosts": hosts[:40],
}
Path(r"C:\Users\kiril\AppData\Local\Temp\hubcap_verbose.json").write_text(
    json.dumps(summary, indent=2), encoding="utf-8"
)

for p in out:
    print(f"{p['status']:>4} {p['path']:<70}")
    print(f"     H: {p['headers']}")
    print(f"     {p['snip'][:160]}")
print("\nADMIN PAGES", admin_pages)
print("DISCORD", discord_links)
print("ENV", summary["env_like"])
print("CDN", cdn)
print("HOSTS", hosts[:20])
