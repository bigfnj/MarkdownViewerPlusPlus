#!/usr/bin/env python3
"""Check whether newer versions of pinned native dependencies are available.

Reads the pinned MERMAID_VERSION, WEBVIEW2_VERSION, CMARK_GFM_VERSION
constants from install-native-deps.py and compares each against the
latest release published upstream:

- mermaid       → npm registry (registry.npmjs.org)
- cmark-gfm     → GitHub releases (github/cmark-gfm)
- WebView2 SDK  → NuGet flat container (Microsoft.Web.WebView2, stable only)

Pure-Python; no third-party deps. Exit 0 always — this is informational and
must not break local dev/CI when a registry is briefly unreachable. Bumping
a pinned version is still a deliberate change to install-native-deps.py.
"""

from __future__ import annotations

import json
import re
import sys
import urllib.request
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PINS_FILE = SCRIPT_DIR / "install-native-deps.py"


def read_pinned() -> dict[str, str]:
    text = PINS_FILE.read_text()
    pins: dict[str, str] = {}
    for name in ("MERMAID_VERSION", "WEBVIEW2_VERSION", "CMARK_GFM_VERSION"):
        match = re.search(rf'^{name}\s*=\s*"([^"]+)"', text, re.MULTILINE)
        if match:
            pins[name] = match.group(1)
    return pins


def fetch_json(url: str, timeout: int = 15):
    request = urllib.request.Request(
        url, headers={"User-Agent": "markdownplusplus-check-deps"}
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.load(response)


def latest_mermaid() -> str:
    return fetch_json("https://registry.npmjs.org/mermaid/latest")["version"]


def latest_cmark_gfm() -> str:
    data = fetch_json("https://api.github.com/repos/github/cmark-gfm/releases/latest")
    return data["tag_name"].lstrip("v")


def latest_webview2() -> str:
    data = fetch_json(
        "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/index.json"
    )
    # NuGet flat container returns versions oldest-first. Filter out pre-release
    # tags (e.g. "1.0.4000-prerelease") so we only surface stable upgrades.
    stable = [v for v in data.get("versions", []) if "-" not in v]
    return stable[-1] if stable else ""


CHECKS = (
    ("mermaid",      "MERMAID_VERSION",   latest_mermaid),
    ("cmark-gfm",    "CMARK_GFM_VERSION", latest_cmark_gfm),
    ("WebView2 SDK", "WEBVIEW2_VERSION",  latest_webview2),
)


def main() -> int:
    pins = read_pinned()
    print(f"{'dep':<14}  {'pinned':<14}  {'latest':<14}  status")
    print(f"{'---':<14}  {'------':<14}  {'------':<14}  ------")
    for label, key, fetcher in CHECKS:
        pinned = pins.get(key, "?")
        try:
            latest = fetcher()
        except Exception as exc:
            print(f"{label:<14}  {pinned:<14}  {'?':<14}  fetch failed: {exc}")
            continue
        status = "up-to-date" if pinned == latest else "→ UPDATE AVAILABLE"
        print(f"{label:<14}  {pinned:<14}  {latest:<14}  {status}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
