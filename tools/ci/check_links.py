#!/usr/bin/env python3
"""Minimal markdown link checker for ci-docs (spec C).

Checks https?:// URLs found in docs/**/*.md and *.md with HEAD (fallback GET),
3 attempts, tolerant of 429. Localhost/example/VERIFY links are skipped.

Usage: check_links.py [file-or-dir ...]
"""

from __future__ import annotations

import concurrent.futures as cf
import re
import sys
import urllib.request
from pathlib import Path

SKIP_PATTERNS = ("localhost", "127.0.0.1", "example.com", "10.0.2.2", "your-")
URL_RE = re.compile(r"https?://[^\s\)\]>\"']+")


def check(url: str) -> tuple[str, int | str]:
    if any(p in url for p in SKIP_PATTERNS):
        return url, 0  # skipped
    req = urllib.request.Request(url, method="HEAD", headers={"User-Agent": "pd-link-check/0.1"})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=15) as resp:
                return url, resp.status
        except Exception as exc:  # noqa: BLE001 - report any failure
            if "HTTP Error" in str(exc) and ("429" in str(exc) or "403" in str(exc)):
                return url, 0  # rate-limited/blocked: tolerate
            if attempt == 2:
                try:  # some hosts reject HEAD
                    req2 = urllib.request.Request(url, headers={"User-Agent": "pd-link-check/0.1"})
                    with urllib.request.urlopen(req2, timeout=15) as resp:
                        return url, resp.status
                except Exception:  # noqa: BLE001
                    return url, str(exc)
    return url, "unreachable"


def main() -> int:
    targets = sys.argv[1:] or ["docs", "README.md"]
    urls: set[str] = set()
    for t in targets:
        p = Path(t)
        files = [p] if p.is_file() else list(p.rglob("*.md"))
        for f in files:
            try:
                urls.update(URL_RE.findall(f.read_text(encoding="utf-8")))
            except OSError:
                continue

    bad: list[str] = []
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        for url, status in ex.map(check, sorted(urls)):
            if status in (0, 200, 301, 302, 303, 307, 308, 403, 429, 999):
                continue
            bad.append(f"{url} -> {status}")

    print(f"check_links: {len(urls)} urls checked, {len(bad)} failures")
    for b in bad:
        print(f"  - {b}")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
