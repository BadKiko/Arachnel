#!/usr/bin/env python3
"""Hubcap leftover/debug probe — free/noauth only. No manifest/lua/generate downloads."""
from __future__ import annotations

import json
import ssl
import sys
import urllib.error
import urllib.request
from pathlib import Path

BASE = "https://hubcapmanifest.com"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("hubcap_probe.json")

# Owner-authorized audit key — used only for max 5 clearly-free authz tests later.
API_KEY = None  # set by caller via env in second script

PATHS = [
    "/.env",
    "/.env.local",
    "/.env.production",
    "/.git/HEAD",
    "/.git/config",
    "/.gitignore",
    "/assets/index-CBIkeEhi.js.map",
    "/assets/index-D37AXcvF.css.map",
    "/swagger",
    "/swagger/",
    "/swagger-ui",
    "/swagger-ui/",
    "/swagger.json",
    "/openapi.json",
    "/openapi.yaml",
    "/docs",
    "/docs/",
    "/redoc",
    "/api/docs",
    "/api/swagger",
    "/graphql",
    "/graphiql",
    "/playground",
    "/metrics",
    "/prometheus",
    "/actuator",
    "/actuator/health",
    "/actuator/env",
    "/actuator/info",
    "/debug",
    "/debug/",
    "/debug/pprof",
    "/__debug",
    "/_debug",
    "/phpinfo.php",
    "/server-status",
    "/server-info",
    "/health",
    "/healthz",
    "/ready",
    "/readyz",
    "/live",
    "/status",
    "/ping",
    "/api/v1/health",
    "/api/health",
    "/api/status",
    "/api/v1/status",
    "/internal",
    "/internal/",
    "/api/internal",
    "/api/v0",
    "/api/v0/",
    "/legacy",
    "/test",
    "/api/test",
    "/backup",
    "/dump",
    "/export",
    "/db.sql",
    "/database.sql",
    "/backup.sql",
    "/.DS_Store",
    "/admin",
    "/admin/",
    "/admin/dumping",
    "/admin/keys",
    "/admin/users",
    "/admin/requests",
    "/admin/depots",
    "/admin/games",
    "/admin/lua",
    "/admin/manifests",
    "/admin/cleanfiles",
    "/admin/announce",
    "/admin/dlc",
    "/admin/refusals",
    "/admin/app-releases",
    "/admin/client-releases",
    "/admin/api-whitelists",
    "/admin/ua-blocklist",
    "/admin/hubcap-whitelist",
    "/admin/hubcap-ticket-urls",
    "/admin/force-role-refresh",
    "/admin-api",
    "/admin-api/",
    "/admin-api/health",
    "/admin-api/status",
    "/admin-api/users",
    "/api/admin",
    "/api/admin/hubcap-whitelist",
    "/api/admin/hubcap-ticket-urls",
    "/api/v1/admin",
    "/api/v1/admin/scan-adult-content/status",
    "/auth/me",
    "/api/auth/me",
    "/login",
    "/api/login",
    "/static/css/style.css",
    "/robots.txt",
    "/sitemap.xml",
    "/favicon.ico",
    "/.well-known/security.txt",
    "/security.txt",
    "/VERSION",
    "/version",
    "/api/version",
    "/api/v2/client/latest?channel=stable",
    "/api/v2/app/latest?channel=stable",
    "/api/v2/client/web-prepare?channel=stable",
    "/api/v2/app/web-prepare?channel=stable",
    "/api/stats",
    "/api/filters",
    "/api/library-stats",
    "/api/recent?limit=5",
    "/api/top-users?page=1",
    "/api/unique-apps?page=1",
    "/api/unique-apps?user_id=1",
    "/api/unique-apps?user_id=1327856038967246970",
    "/api/games?sort_by=date_newest&limit=2&offset=0",
    "/api/v1/generate/limits",
    "/api/v1/generate/stats",
    "/api/v1/generate/branches/400",
    "/api/v1/generate/usage",
    "/api/v1/search?q=test&limit=2",
    "/api/v1/library?limit=2&offset=0",
    "/api/v1/user/stats",
    "/api/v1/status/400",
    "/api/depot-keys/check",
    "/api/v1/depot-keys",
    "/api-keys/system-status",
    "/api-keys/pricing",
    "/api-keys/stats",
    "/api-keys/my-key-info",
    "/api-keys/stats/leaderboard?page=1&per_page=5&stat_type=requests",
    "/api/addapp/400",
    "/api/apps?q=portal",
    "/api/search?query=portal",
    "/api/day/2024-01-01",
    "/api/calendar/2024",
    "/api/game/400",
    "/api/apps/400",
    "/api/depot/400",
    "/api/user/usage",
    "/config.json",
    "/env.js",
    "/config.js",
    "/settings.json",
    "/manifest.json",
    "/api/",
    "/api/v1/",
    "/api/v2/",
    "/download/",
    "/download/prepare/400",
]

ctx = ssl.create_default_context()
results = []

for path in PATHS:
    url = BASE + path
    req = urllib.request.Request(
        url,
        headers={"User-Agent": "HubcapLeftoverAudit/1.0 (owner-authorized)"},
        method="GET",
    )
    try:
        with urllib.request.urlopen(req, context=ctx, timeout=20) as resp:
            body = resp.read(400)
            snip = body.decode("utf-8", errors="replace").replace("\n", " ")[:180]
            results.append(
                {
                    "path": path,
                    "status": resp.status,
                    "ctype": resp.headers.get("Content-Type", ""),
                    "len": resp.headers.get("Content-Length"),
                    "snip": snip,
                    "interesting": True,
                }
            )
    except urllib.error.HTTPError as e:
        body = e.read(400)
        snip = body.decode("utf-8", errors="replace").replace("\n", " ")[:180]
        # Keep 401/403/200/301/302 and non-SPA 404s that look like JSON APIs
        keep = e.code not in (404,) or path.startswith(
            ("/.env", "/.git", "/swagger", "/metrics", "/debug", "/admin", "/api/")
        )
        # SPA catch-all often returns HTML 200 for unknown paths; 404 HTML is noise
        is_html_404 = e.code == 404 and "text/html" in (e.headers.get("Content-Type") or "")
        if keep and not (is_html_404 and not path.startswith(("/api", "/admin-api", "/.env", "/.git", "/assets/"))):
            results.append(
                {
                    "path": path,
                    "status": e.code,
                    "ctype": e.headers.get("Content-Type", ""),
                    "len": e.headers.get("Content-Length"),
                    "snip": snip,
                    "interesting": e.code != 404,
                }
            )
        elif e.code != 404:
            results.append(
                {
                    "path": path,
                    "status": e.code,
                    "ctype": e.headers.get("Content-Type", ""),
                    "len": e.headers.get("Content-Length"),
                    "snip": snip,
                    "interesting": True,
                }
            )
    except Exception as ex:
        results.append(
            {
                "path": path,
                "status": "ERR",
                "ctype": "",
                "len": None,
                "snip": str(ex)[:180],
                "interesting": True,
            }
        )

OUT.write_text(json.dumps(results, indent=2), encoding="utf-8")
# Print interesting first
interesting = [r for r in results if r.get("interesting") or r["status"] not in (404, "404")]
print(f"total recorded={len(results)} interesting={len(interesting)}")
for r in results:
    flag = "*" if r.get("interesting") or r["status"] not in (404, "404") else " "
    print(f"{flag} {r['status']:>4} {r['path']:<70} {r['snip'][:100]}")
