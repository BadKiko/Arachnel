#!/usr/bin/env python3
"""Full free bodies + max 5 API-key authz tests (free endpoints only)."""
from __future__ import annotations

import json
import ssl
import urllib.error
import urllib.request
from pathlib import Path

BASE = "https://hubcapmanifest.com"
# Redact in printed output
KEY = "smm_b6589c0e782b4bb9ce43aac46e318848d1b871fc49f925ceaa5f772f8be8c4efa0252638dd67298c2d4696ca3d2113d2"
KEY_REDACTED = "smm_b658…e8c4efa0…3d2113d2"
ctx = ssl.create_default_context()


def fetch(path: str, headers: dict | None = None, method: str = "GET", data: bytes | None = None) -> dict:
    h = {"User-Agent": "HubcapLeftoverAudit/1.0 (owner-authorized)"}
    if headers:
        h.update(headers)
    req = urllib.request.Request(BASE + path, data=data, headers=h, method=method)
    try:
        with urllib.request.urlopen(req, context=ctx, timeout=25) as resp:
            body = resp.read(4000)
            return {
                "path": path,
                "status": resp.status,
                "ctype": resp.headers.get("Content-Type", ""),
                "x_client_ip": resp.headers.get("X-Client-Ip"),
                "body": body.decode("utf-8", errors="replace"),
            }
    except urllib.error.HTTPError as e:
        body = e.read(4000)
        return {
            "path": path,
            "status": e.code,
            "ctype": e.headers.get("Content-Type", ""),
            "x_client_ip": e.headers.get("X-Client-Ip"),
            "body": body.decode("utf-8", errors="replace"),
        }


results = {}

# Full free bodies of interest
for p in [
    "/health",
    "/api/v1/health",
    "/billing/tiers",
    "/api/v1/generate/branches/400",
    "/api/v2/client/latest?channel=stable",
    "/auth/role-name/1327860222068527144",
    "/api/v1/generate/limits",
]:
    results[p] = fetch(p)

# Authz budget: exactly 5 free/admin-status calls with normal API key
auth_tests = [
    # 1 system status / key info
    ("/api-keys/my-key-info", {}),
    # 2 admin all-users (authz gap test)
    ("/api-keys/admin/all-users", {}),
    # 3 admin scan status
    ("/api/v1/admin/scan-adult-content/status", {}),
    # 4 generate limits for another user (IDOR)
    ("/api/v1/generate/limits?user_id=1", {}),
    # 5 hubcap admin whitelist
    ("/api/admin/hubcap-whitelist", {}),
]

auth_results = []
for path, extra in auth_tests:
    # Prefer X-API-Key header
    r = fetch(path, headers={"X-API-Key": KEY, **extra})
    # Redact any key echo
    body = r["body"].replace(KEY, KEY_REDACTED)
    auth_results.append(
        {
            "path": path,
            "status": r["status"],
            "ctype": r["ctype"],
            "x_client_ip": r["x_client_ip"],
            "body": body[:2500],
        }
    )

# Also check Accept: application/json on paths that returned SPA HTML
json_accept = []
for p in [
    "/api/stats",
    "/api/top-users?page=1",
    "/api/unique-apps?page=1",
    "/api/unique-apps?user_id=1",
    "/api/recent?limit=3",
    "/api/games?sort_by=date_newest&limit=2&offset=0",
    "/api/apps?q=portal",
    "/api/search?query=portal",
    "/api/game/400",
    "/api/filters",
    "/manifest-history/api/stats",
    "/api-keys/pricing",
    "/api-keys/stats",
    "/api-keys/system-status",
]:
    r = fetch(p, headers={"Accept": "application/json"})
    json_accept.append(
        {
            "path": p,
            "status": r["status"],
            "ctype": r["ctype"],
            "snip": r["body"][:300].replace("\n", " "),
        }
    )

out = {
    "free_full": {k: {**v, "body": v["body"][:3000]} for k, v in results.items()},
    "authz_5": auth_results,
    "json_accept": json_accept,
}
Path(r"C:\Users\kiril\AppData\Local\Temp\hubcap_authz.json").write_text(
    json.dumps(out, indent=2), encoding="utf-8"
)

print("=== FREE FULL ===")
for k, v in results.items():
    print(f"\n--- {v['status']} {k} X-Client-Ip={v.get('x_client_ip')} ---")
    print(v["body"][:2000])

print("\n=== AUTHZ 5 (key redacted) ===")
for r in auth_results:
    print(f"\n--- {r['status']} {r['path']} ---")
    print(r["body"][:1500])

print("\n=== JSON ACCEPT ===")
for r in json_accept:
    print(f"{r['status']:>4} {r['path']:<55} {r['snip'][:120]}")
