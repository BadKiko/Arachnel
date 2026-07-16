#!/usr/bin/env python3
"""Extra targeted probes from JS discoveries + IDOR checks. Free only."""
from __future__ import annotations

import json
import ssl
import urllib.error
import urllib.request
from pathlib import Path

BASE = "https://hubcapmanifest.com"
ctx = ssl.create_default_context()
UA = {"User-Agent": "HubcapLeftoverAudit/1.0 (owner-authorized)"}


def get(path: str, headers: dict | None = None) -> dict:
    h = dict(UA)
    if headers:
        h.update(headers)
    req = urllib.request.Request(BASE + path, headers=h, method="GET")
    try:
        with urllib.request.urlopen(req, context=ctx, timeout=25) as resp:
            body = resp.read(800)
            return {
                "path": path,
                "status": resp.status,
                "ctype": resp.headers.get("Content-Type", ""),
                "snip": body.decode("utf-8", errors="replace").replace("\n", " ")[:350],
                "hdrs": {
                    k: resp.headers.get(k)
                    for k in (
                        "X-Client-Ip",
                        "Server",
                        "Via",
                        "WWW-Authenticate",
                        "X-Request-Id",
                        "X-Powered-By",
                    )
                    if resp.headers.get(k)
                },
            }
    except urllib.error.HTTPError as e:
        body = e.read(800)
        return {
            "path": path,
            "status": e.code,
            "ctype": e.headers.get("Content-Type", ""),
            "snip": body.decode("utf-8", errors="replace").replace("\n", " ")[:350],
            "hdrs": {
                k: e.headers.get(k)
                for k in (
                    "X-Client-Ip",
                    "Server",
                    "Via",
                    "WWW-Authenticate",
                    "X-Request-Id",
                    "X-Powered-By",
                )
                if e.headers.get(k)
            },
        }
    except Exception as ex:
        return {"path": path, "status": "ERR", "ctype": "", "snip": str(ex)[:350], "hdrs": {}}


def post(path: str, data: bytes | None = None, headers: dict | None = None) -> dict:
    h = dict(UA)
    if headers:
        h.update(headers)
    req = urllib.request.Request(BASE + path, data=data or b"{}", headers=h, method="POST")
    try:
        with urllib.request.urlopen(req, context=ctx, timeout=25) as resp:
            body = resp.read(800)
            return {
                "path": "POST " + path,
                "status": resp.status,
                "ctype": resp.headers.get("Content-Type", ""),
                "snip": body.decode("utf-8", errors="replace").replace("\n", " ")[:350],
                "hdrs": {},
            }
    except urllib.error.HTTPError as e:
        body = e.read(800)
        return {
            "path": "POST " + path,
            "status": e.code,
            "ctype": e.headers.get("Content-Type", ""),
            "snip": body.decode("utf-8", errors="replace").replace("\n", " ")[:350],
            "hdrs": {},
        }
    except Exception as ex:
        return {"path": "POST " + path, "status": "ERR", "ctype": "", "snip": str(ex)[:350], "hdrs": {}}


extra = [
    "/manifest-history/api/stats",
    "/manifest-history/api/recent?limit=5",
    "/manifest-history/api/depot-keys/check",
    "/billing/tiers",
    "/auth/role-name/1327860222068527144",
    "/auth/role-name/1392815195096748115",
    "/auth/refresh-roles",
    "/auth/admin/force-role-refresh",
    "/auth/admin/role-info/1",
    "/cleanfiles-api/run",
    "/depot/detection",
    "/depot/external-check",
    "/api/v1/generate/limits?user_id=1",
    "/api/v1/generate/limits?user_id=999999999999999999",
    "/api/v1/generate/limits?user_id=1327856038967246970",
    "/api/unique-apps?user_id=1",
    "/api/unique-apps?user_id=2",
    "/api/top-users?page=1",
    "/api/user/usage",
    "/api/v1/user/stats",
    "/api-keys/admin/all-users",
    "/api-keys/admin/pending-requests",
    "/api-keys/admin/cleanup-expired",
    "/api/admin/hubcap-whitelist",
    "/api/admin/hubcap-ticket-urls",
    "/admin-api/dump/source-status",
    "/admin-api/dump/keys",
    "/admin-api/lua/test",
    "/admin-api/lua/status",
    "/admin-api/announce",
    "/admin-api/users/1",
    "/admin-api/requests",
    "/admin-api/scan",
    "/api/v1/admin/scan-adult-content/status",
    "/api/v1/health",
    "/api/v1/generate/stats",
    "/api/v1/generate/limits",
    "/api/v1/generate/branches/400",
    "/api/v1/status/400",
    "/api/v1/search?q=a&limit=1",
    "/api/v1/library?limit=1&offset=0",
    "/api/v2/client/latest?channel=stable",
    "/api/v2/app/latest?channel=beta",
    "/api/v2/client/latest?channel=dev",
    "/api/stats",
    "/api/filters",
    "/api/library-stats",
    "/api/recent?limit=3",
    "/api-keys/system-status",
    "/api-keys/pricing",
    "/api-keys/stats",
    "/api-keys/my-key-info",
    "/api/depot-keys/check",
    "/api/v1/depot-keys",
    "/.env",
    "/.git/HEAD",
    "/assets/index-CBIkeEhi.js.map",
    "/swagger.json",
    "/openapi.json",
    "/metrics",
    "/debug",
    "/graphql",
    "/phpinfo.php",
    "/server-status",
    "/actuator/env",
    "/health",
    "/api/health",
    "/X-Client-Ip-check-via-health",  # placeholder skipped
]

results = []
for p in extra:
    if p.startswith("/X-"):
        continue
    results.append(get(p))

# A few POSTs that should not consume downloads
results.append(post("/cleanfiles-api/run", b"{}", {"Content-Type": "application/json"}))
results.append(post("/auth/refresh-roles", b"{}", {"Content-Type": "application/json"}))
results.append(post("/download/prepare/400", b"{}", {"Content-Type": "application/json"}))
results.append(post("/api/depot-keys/check", b'{"depot_id":401}', {"Content-Type": "application/json"}))
results.append(
    post(
        "/manifest-history/api/depot-keys/check",
        b'{"depot_id":401}',
        {"Content-Type": "application/json"},
    )
)

out = Path(r"C:\Users\kiril\AppData\Local\Temp\hubcap_extra.json")
out.write_text(json.dumps(results, indent=2), encoding="utf-8")
for r in results:
    print(f"{r['status']:>4} {r['path']:<75} {r['snip'][:140]}")
