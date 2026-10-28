#!/usr/bin/env python3
"""Generate release.json describing a flasher channel build (spec E2).

Sits next to manifest.json (but is NOT part of it) and carries: version,
channel, git SHA, build date, IDF version, SHA256 of every part and of the
merged .bin, and the changelog markdown from release-please.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--channel", choices=["stable", "beta", "dev"], required=True)
    ap.add_argument("--version", required=True)
    ap.add_argument("--git-sha", default="")
    ap.add_argument("--idf-version", default="v5.5.6")
    ap.add_argument("--merged-bin", default="", help="path to the merged image (optional)")
    ap.add_argument("--changelog", default="", help="path to a changelog markdown fragment")
    ap.add_argument("--out", default="release.json")
    ap.add_argument("parts", nargs="*", help="part files to hash (bootloader.bin app.bin ...)")
    args = ap.parse_args()

    git_sha = args.git_sha
    if not git_sha:
        try:
            git_sha = subprocess.check_output(
                ["git", "rev-parse", "--short", "HEAD"], text=True
            ).strip()
        except Exception:  # noqa: BLE001
            git_sha = "unknown"

    changelog = ""
    if args.changelog:
        p = Path(args.changelog)
        if p.exists():
            changelog = p.read_text(encoding="utf-8")

    release = {
        "version": args.version,
        "channel": args.channel,
        "git_sha": git_sha,
        "build_date": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "idf_version": args.idf_version,
        "sha256": {p: sha256_of(Path(p)) for p in args.parts},
        "changelog": changelog,
    }

    if args.merged_bin:
        release["merged_bin_sha256"] = sha256_of(Path(args.merged_bin))

    Path(args.out).write_text(json.dumps(release, indent=2) + "\n", encoding="utf-8")
    print(f"release.json written to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
