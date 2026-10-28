# Release Process

Status: Phase 0.5 design; the pipeline activates with `vars.ENABLE_RELEASE_PIPELINE=true` (Phase 6).

This document describes how a commit on `main` becomes a tagged release, which
artifacts the pipeline produces, how the web-flasher channels are fed, and how
updates are rolled out and rolled back. Signing-key handling lives in
`security-keys.md`.

## 1. Conventional commits and versioning

- Every PR title must follow Conventional Commits; `pr-title.yml`
  (`amannn/action-semantic-pull-request`) enforces it. Squash-merge commit
  messages must match the PR title.
- `feat` / `fix` / `perf` drive version bumps; `chore`, `docs`, `test` do not.
  Breaking changes use `!` or a `BREAKING CHANGE:` footer.
- release-please runs in manifest mode over 5 packages: `firmware-main`,
  `firmware-coproc`, `android-app`, `cloud`, `web-flasher`
  (`release-please-config.json` + `.release-please-manifest.json`).
- Flow: merging `feat`/`fix` commits to `main` makes release-please open or
  update a per-package release PR (version bump + changelog). Merging that PR
  publishes the per-package GitHub Release; `release.yml` reacts to
  `release: published`.
- `firmware-main` version is written to `firmware-main/version.txt`, which is
  compiled in as `PROJECT_VER` and surfaces on the device About screen and in
  MQTT telemetry.

Semantic versioning policy for device firmware:

| Bump | Meaning |
| --- | --- |
| MAJOR | Incompatible change: partition layout, RS485 protocol version, storage schema without migration, breaking cloud API |
| MINOR | New features, new screens, new telemetry fields, schema migrations that remain compatible |
| PATCH | Bug fixes with no format or behavior contract changes |

## 2. Pipeline overview

`release.yml` is gated by `vars.ENABLE_RELEASE_PIPELINE`; until Phase 6 it
self-skips with a notice. On `release: published` it runs, split into jobs so
that only the `sign` job ever touches key material.

## 3. `release.yml` step by step

1. **Build release-unsigned** — firmware-main built with `sdkconfig.release`
   (secure boot v2 + flash encryption RELEASE settings, `PD_HAL_REAL=y`,
   `PD_SIM_CONSOLE=n`), images left unsigned. `tools/ci/check_release_sdkconfig.py`
   verifies the sdkconfig hash of the build.
2. **Sign** — job in the `firmware-signing` environment (required reviewers,
   restricted to release tags matching `v*`). Signs bootloader + app with the
   secure boot v2 key from KMS/HSM via OIDC (espsecure — VERIFY: confirm
   espsecure v5.x HSM/PKCS#11 syntax), or from the fallback environment secret
   (base64 decoded to tmpfs, used once, shredded). Produces `*-prod-signed.bin`.
3. **Build dev images** — firmware-main built with `sdkconfig.defaults`:
   NO secure boot, NO flash encryption. These images feed the web flasher.
4. **Merge + manifest** — `tools/flasher/merge_bin.sh` produces the merged dev
   image; `tools/flasher/gen_manifest.py` generates `manifest.json` from
   `build/flasher_args.json`.
5. **SBOM, checksums, provenance** — `idf.py sbom-create` (`.spdx`),
   `SHA256SUMS` over every asset, `actions/attest-build-provenance` for each
   binary.
6. **Android** — `assembleRelease`/`bundleRelease` signed with the upload key
   from the `android-release` environment; optional Firebase App Distribution
   via WIF.
7. **Upload assets** — attach the exact asset list to the GitHub Release.

Acceptance (spec C8): a tag produces all assets + checksums + SBOM +
provenance; no signing key ever appears in logs or artifacts.

## 4. Release assets

| Asset | Purpose | Channel |
| --- | --- | --- |
| `pd-main-<ver>-dev-merged.bin` | Merged dev image for first install / esptool fallback | dev / beta / stable pages |
| `pd-main-<ver>-dev-parts.zip` | Parts (bootloader, partition table, otadata, app, ota_data_initial) for keep-settings updates | dev / beta / stable pages |
| `pd-main-<ver>-ota-prod-signed.bin` | Signed OTA image for hardened devices | signed OTA only |
| `coproc-<ver>.uf2` / `coproc-<ver>.bin` | Co-processor firmware | Phase 3 |
| `app-<ver>.apk` | Android app | Play / page download |
| `*.spdx` | SBOM | release assets |
| `SHA256SUMS` | Checksums over every asset | release assets |
| `manifest.json` | ESP Web Tools manifest | web flasher |

Dev-channel images are built WITHOUT secure boot and WITHOUT flash
encryption. Production-hardened images are never published to the web flasher.

## 5. Channels and Pages publishing

| Channel | Content | Marking |
| --- | --- | --- |
| `stable/` | Latest non-prerelease Release (dev-config images) | default button |
| `beta/` | Latest prerelease | beta |
| `dev/` | Last green `main` build | clearly marked "development" |

- `pages.yml` regenerates manifests and release.json, verifies SHA256, and
  deploys the site; all binaries are copied into the Pages artifact
  (same origin), never linked from Release asset URLs.
- Retention: keep the last 3 versions per channel to stay well under the 1 GB
  Pages limit; older versions link to their GitHub Releases.
- Hardened (secure boot + flash encryption release mode) images are NEVER
  offered on the page; those devices update via signed OTA only, because a
  browser flash of plaintext/unsigned images fails on a hardened device.

## 6. Rollout and rollback

- **Staged rollout**: a new signed OTA image goes first to a small cohort
  (bench and owned devices), then widens in steps; each step is gated on the
  device health signals reported over MQTT (version distribution, rollback
  count, crash loops).
- **Self-test and rollback**: the new app slot must pass its self-test and
  then call `esp_ota_mark_app_valid_cancel_rollback()`; if the device reboots
  before that, the bootloader rolls back to the previous slot automatically.
  Both paths are covered by `pytest_ota_rollback_qemu.py` and re-verified on
  HIL before a tag is cut.
- **Key loss warning**: a lost signing key means no further OTA for hardened
  devices (secure boot v2 rejects anything not signed by the burned key
  digest). The offline two-person sealed backup in `security-keys.md` exists
  precisely for this; losing both the live key and the backup bricks OTA for
  the fleet of hardened devices permanently.

## 7. Hotfix path

1. Branch `hotfix/<pkg>-v<ver>` from the affected release tag.
2. Cherry-pick the fix commits (they keep their Conventional Commit messages)
   via normal PRs into the hotfix branch.
3. Publish the fix first as a **prerelease**: `release.yml` builds and the
   `beta/` channel picks it up; validate on bench + HIL.
4. Publish the final patch release; `stable/` and the signed OTA rollout pick
   it up through the normal pipeline.
5. If the fix touches `fsm`, `scheduler`, `ota` or dose logic, the `safety`
   label and FMEA review are mandatory before step 4.
