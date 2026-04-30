#!/usr/bin/env python3
"""Install repo-local native dependencies for Markdown++.

This script intentionally writes only under the repository root:

- packages/Microsoft.Web.WebView2.<version>
- tools/nuget/nuget.exe
- third_party/cmark-gfm
- MarkdownPlusPlus.Native/assets/mermaid/mermaid.min.js
"""

from __future__ import annotations

import json
import shutil
import tarfile
import urllib.request
import zipfile
from pathlib import Path


WEBVIEW2_VERSION = "1.0.3912.50"
CMARK_GFM_VERSION = "0.29.0.gfm.13"
MERMAID_VERSION = "11.14.0"


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and destination.stat().st_size > 0:
        return

    temporary = destination.with_suffix(destination.suffix + ".tmp")
    with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as output:
        shutil.copyfileobj(response, output)
    temporary.replace(destination)


def install_webview2(root: Path, cache: Path) -> None:
    package_dir = root / "packages" / f"Microsoft.Web.WebView2.{WEBVIEW2_VERSION}"
    nupkg = cache / f"Microsoft.Web.WebView2.{WEBVIEW2_VERSION}.nupkg"
    download(
        f"https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/{WEBVIEW2_VERSION}/"
        f"microsoft.web.webview2.{WEBVIEW2_VERSION}.nupkg",
        nupkg,
    )

    if package_dir.exists():
        return

    package_dir.mkdir(parents=True)
    with zipfile.ZipFile(nupkg) as archive:
        archive.extractall(package_dir)


def install_nuget(root: Path) -> None:
    download("https://dist.nuget.org/win-x86-commandline/latest/nuget.exe", root / "tools" / "nuget" / "nuget.exe")


def install_cmark_gfm(root: Path, cache: Path) -> None:
    release_url = f"https://api.github.com/repos/github/cmark-gfm/tarball/{CMARK_GFM_VERSION}"
    tarball = cache / f"cmark-gfm-{CMARK_GFM_VERSION}.tar.gz"
    destination = root / "third_party" / "cmark-gfm"

    download(release_url, tarball)

    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)

    with tarfile.open(tarball, "r:gz") as archive:
        members = archive.getmembers()
        prefix = members[0].name.split("/")[0] + "/"
        for member in members:
            if not member.name.startswith(prefix):
                continue
            member.name = member.name[len(prefix) :]
            if member.name:
                archive.extract(member, destination)

    patch_cmark_gfm_cmake(destination)


def patch_cmark_gfm_cmake(destination: Path) -> None:
    """Keep vendored cmark-gfm compatible with CMake 4+ / VS2026.

    Upstream cmark-gfm 0.29.0.gfm.13 still declares compatibility with CMake
    3.0. CMake 4 removed compatibility with versions older than 3.5 and warns
    on similarly old compatibility floors, so the VS2026 configure output is
    noisy unless we raise the floor.
    """

    cmake_file = destination / "CMakeLists.txt"
    text = cmake_file.read_text(encoding="utf-8")
    text = text.replace(
        "cmake_minimum_required(VERSION 3.0)",
        "cmake_minimum_required(VERSION 3.10)",
        1,
    )
    text = text.replace(
        "cmake_minimum_required(VERSION 3.5)",
        "cmake_minimum_required(VERSION 3.10)",
        1,
    )
    cmake_file.write_text(text, encoding="utf-8")


def install_mermaid(root: Path, cache: Path) -> None:
    metadata_url = f"https://registry.npmjs.org/mermaid/{MERMAID_VERSION}"
    with urllib.request.urlopen(metadata_url, timeout=30) as response:
        metadata = json.load(response)

    tarball = cache / f"mermaid-{MERMAID_VERSION}.tgz"
    download(metadata["dist"]["tarball"], tarball)

    destination = root / "MarkdownPlusPlus.Native" / "assets" / "mermaid" / "mermaid.min.js"
    destination.parent.mkdir(parents=True, exist_ok=True)

    with tarfile.open(tarball, "r:gz") as archive:
        member = next((item for item in archive.getmembers() if item.name == "package/dist/mermaid.min.js"), None)
        if member is None:
            raise RuntimeError("Could not find package/dist/mermaid.min.js in the Mermaid package.")

        with archive.extractfile(member) as source, destination.open("wb") as output:
            if source is None:
                raise RuntimeError("Could not read package/dist/mermaid.min.js from the Mermaid package.")
            shutil.copyfileobj(source, output)


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    cache = root / ".download-cache"
    cache.mkdir(exist_ok=True)

    install_webview2(root, cache)
    install_nuget(root)
    install_cmark_gfm(root, cache)
    install_mermaid(root, cache)
    shutil.rmtree(cache, ignore_errors=True)

    print(f"Microsoft.Web.WebView2 {WEBVIEW2_VERSION}")
    print(f"cmark-gfm {CMARK_GFM_VERSION}")
    print(f"Mermaid {MERMAID_VERSION}")
    print("NuGet CLI latest")


if __name__ == "__main__":
    main()
