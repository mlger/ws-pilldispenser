# CI/CD and Automation

Status: Phase 0.5. All workflows are SHA-pinned; Dependabot bumps the pins weekly.

This is the operating manual for `.github/workflows/`. It records the workflow
inventory, the global hardening rules (master-spec Section C0), the pinned
action versions, caching, the ESP-IDF pin and canary policy, branch protection,
repository switches, and the consolidated VERIFY register (spec G1: every
unverified assumption lands here).

## 1. Workflow inventory

| Workflow | Triggers | Key jobs | Acceptance criteria |
| --- | --- | --- | --- |
| `ci-firmware-main.yml` | PR + push(main); paths `firmware-main/**`, `tools/**`, itself | `lint`, `host-tests`, `build` (matrix: dev / qemu / release-unsigned), `size-report`, `qemu-tests`, `sbom` | A PR touching firmware-main cannot merge unless lint, host-tests, all build legs, size-report and qemu-tests pass |
| `ci-coproc.yml` | PR + push(main); paths `firmware-coproc/**` | Gated: self-skips until `firmware-coproc` has a buildable project; then build + shared `core_protocol` codec tests | Build green; codec tests identical on both sides of the RS485 link (once active) |
| `ci-android.yml` | PR + push(main); paths `android-app/**` | Gated: self-skips until `android-app` has Gradle files; then ktlint, detekt, lint, unit tests, `assembleDebug` | No new lint/detekt issues; unit tests green; debug APK artifact |
| `ci-cloud.yml` | PR + push(main); paths `cloud/**` | Gated: self-skips until `cloud` has `package.json` files; then npm ci, lint, typecheck, unit tests, Firestore rules emulator tests | 100% of rules test cases pass; this workflow never deploys |
| `ci-web-flasher.yml` | PR; paths `web-flasher/**`, `tools/flasher/**` | Site build/lint, manifest contract tests (ESP32-S3 bootloader offset must be `0`, never `4096`) | Manifest contract tests pass before the site can merge |
| `ci-docs.yml` | PR; paths `docs/**`, `**/*.md` | markdownlint-cli2, link checker (`tools/ci/check_links.py`) | Markdown clean, no broken links |
| `codeql.yml` | PR + push(main) + weekly schedule | CodeQL: `actions`, `javascript-typescript`, `python`, `c-cpp` (manual build of `host_test`) | No new high/critical alerts introduced by a PR |
| `dependency-review.yml` | `pull_request` | `actions/dependency-review-action`: deny GPL-3.0, fail on high/critical, comment summary | PR blocked on license or severity violation |
| `scorecard.yml` | Weekly schedule | OSSF Scorecard scan | Score tracked; regressions triaged |
| `release-please.yml` | push(main) | release-please, manifest mode, 5 packages (`firmware-main`, `firmware-coproc`, `android-app`, `cloud`, `web-flasher`) | Version bumps and changelogs maintained without manual edits |
| `release.yml` | `release: published` | Build release-unsigned, `sign` (environment `firmware-signing`), dev images, merge + manifest, SBOM/checksums/provenance, Android (environment `android-release`), asset upload | A release produces all assets + checksums + SBOM + provenance; no key in logs or artifacts |
| `pages.yml` | push(main); paths `web-flasher/**` + `workflow_dispatch` (+ `workflow_run` after `release.yml`) | Build Pages site, `gen_manifest.py`, `gen_release_json.py`, SHA256 verify, deploy | Site deployed within the 10-minute budget; binaries served same-origin |
| `deploy-cloud.yml` | push(main); paths `cloud/**` to staging; `release: published` to production | WIF auth, `firebase projects:list` auth check, `firebase deploy` | Production deploy impossible without 2 required reviewers; rules tests green on the same commit |
| `hil.yml` | `workflow_dispatch` + nightly schedule + PR label `run-hil` (same-repo PRs only) | Self-hosted runner `[self-hosted, hil-4.3b]`; flashes DEV builds only; gated by `vars.ENABLE_HIL` | Nightly HIL green before any release tag |
| `nightly.yml` | Schedule | ESP-IDF canary leg (v6.1-rc1, non-blocking), long QEMU soak (`vars.ENABLE_NIGHTLY_SOAK`), SBOM check vs NVD (`vars.ENABLE_SBOM_CHECK`), IDF component-manager drift check | Canary/drift failures open issues; they never block merges |
| `pr-title.yml` | PR opened/edited/synchronized | Conventional-commit title check (`amannn/action-semantic-pull-request`) | Non-conventional PR titles are blocked |

## 2. Global workflow rules

1. Top-level `permissions: {}` (or `contents: read`); widen per job only. The
   default `GITHUB_TOKEN` is read-only at repo level, and "Allow GitHub Actions
   to create and approve pull requests" is off (release-please authenticates
   via its own scoped token).
2. Every `uses:` is pinned to a full 40-char commit SHA with a `# vX.Y.Z`
   comment; Dependabot (github-actions ecosystem) bumps the pins weekly.
3. No `pull_request_target` that checks out or runs PR code. Fork PRs get no
   secrets and never run on self-hosted runners.
4. Untrusted inputs (PR titles, branch names) are passed via `env:`, never
   interpolated directly into `run:`.
5. `concurrency: group: ${{ github.workflow }}-${{ github.ref }}`;
   `cancel-in-progress` for PRs only, never for deploy or release jobs.
6. Every job declares `timeout-minutes`: build 30, QEMU 25, HIL 45.
7. ESP-IDF is pinned to an exact tag; jobs run in `container: espressif/idf:<exact tag>`.
8. ccache is enabled (`IDF_CCACHE_ENABLE=1`) and cached with `actions/cache`;
   Gradle, npm and pip use their setup-action caches (Section 4).
9. Only allow-listed actions are permitted: GitHub-owned, Espressif, Google,
   and explicitly reviewed additions (Section 3).
10. Build artifacts (`.bin`, `flasher_args.json`, `flash_args`,
    `project_description.json`, `.elf`, `.map`) are uploaded with 14-day
    retention; failing QEMU jobs always upload JUnit XML, QEMU stdout and
    `qemu_flash.bin` so a failure can be replayed locally.
11. Every new workflow ships with a trigger list, a permissions block, pinned
    SHAs, a job timeout, and an acceptance-criteria comment at the top
    (spec G1); the acceptance criteria are mirrored in Section 1.

## 3. Action pins

Verified against GitHub; Dependabot keeps them current.

| Action | Pinned SHA | Version |
| --- | --- | --- |
| `actions/checkout` | `fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09` | v5.1.0 |
| `actions/cache` | `0057852bfaa89a56745cba8c7296529d2fc39830` | v4.3.0 |
| `actions/upload-artifact` | `ea165f8d65b6e75b540449e92b4886f43607fa02` | v4.6.2 |
| `actions/download-artifact` | `d3f86a106a0bac45b974a628896c90dbdf5c8093` | v4.3.0 |
| `actions/setup-python` | `a26af69be951a213d495a4c3e4e4022e16d87065` | v5.6.0 |
| `actions/setup-node` | `49933ea5288caeca8642d1e84afbd3f7d6820020` | v4.4.0 |
| `actions/setup-java` | `cf277c60eb25467037889841efdb72551f06f6c3` | v4.9.1 |
| `actions/configure-pages` | `983d7736d9b0ae728b81ab479565c72886d7745b` | v5.0.0 |
| `actions/upload-pages-artifact` | `7b1f4a764d45c48632c6b24a0339c27f5614fb0b` | v4.0.0 |
| `actions/deploy-pages` | `d6db90164ac5ed86f2b6aed7e0febac5b3c0c03e` | v4.0.5 |
| `actions/attest-build-provenance` | `e8998f949152b193b063cb0ec769d69d929409be` | v2 |
| `actions/dependency-review-action` | `2031cfc080254a8a887f58cffee85186f0e49e48` | v4.9.0 |
| `ossf/scorecard-action` | `fa7117db83c52d787ae5a89f350999701505d78e` | v2.5.0 |
| `github/codeql-action` | `4bfa242aa49c5f7c6770b98670d354a7dfac05f8` | v3 |
| `googleapis/release-please-action` | `45996ed1f6d02564a971a2fa1b5860e934307cf7` | v5.0.0 |
| `amannn/action-semantic-pull-request` | `0723387faaf9b38adef4775cd42cfd5155ed6017` | v5.5.3 |
| `google-github-actions/auth` | `c200f3691d83b41bf9bbd8638997a462592937ed` | v2.1.13 |

## 4. Caching strategy

| Cache | Mechanism | Key / notes |
| --- | --- | --- |
| ccache (ESP-IDF builds) | `IDF_CCACHE_ENABLE=1` + `actions/cache` | Keyed on the IDF tag + hash of the sdkconfig defaults files; restored via `restore-keys` prefix match |
| Gradle (Android) | `gradle/actions/setup-gradle` | Activated together with `ci-android.yml`; SHA pin pending — see VERIFY appendix |
| npm (cloud, web-flasher) | `actions/setup-node` with `cache: npm` | Keyed on the lockfile hash |
| pip (pytest-embedded) | `actions/setup-python` with `cache: pip` | Keyed on `firmware-main/test/requirements.txt`; pytest-embedded pinned at 2.9.3 |

## 5. ESP-IDF pinning and canary policy

| Leg | Container image | Policy |
| --- | --- | --- |
| Blocking baseline | `espressif/idf:v5.5.6` | Exact pin; all merge-blocking firmware jobs (CI, release, HIL) |
| Canary | `espressif/idf:v6.1-rc1` | `nightly.yml` only; non-blocking; failures open an issue, never fail CI |

- Floating tags (`latest`, `v5`) are forbidden.
- When the canary leg is green across the full suite (unit, scenario, MQTT,
  OTA, UI), a migration PR bumps the baseline; per Section F this lands in
  Phase 7 ("ESP-IDF v6.x migration when the canary leg is green").
- The IDF component manager is not covered by Dependabot; `nightly.yml` runs
  `idf.py update-dependencies` as a dry run and opens an issue on drift.

## 6. Branch protection and required checks

> DECISION (Phase 0.5): workflows use paths filters (spec Section B), so a
> docs-only PR never runs firmware checks. If we marked firmware jobs as
> required, a docs-only PR could never satisfy them and would hang forever.
> Required status checks are therefore deferred until an aggregate gate exists.

Current branch protection on `main` (Phase 0.5):

- Pull requests required; 1 approval (2 for `firmware-main/components/{fsm,scheduler,ota}`
  and `cloud/firestore.rules`, per spec C9); CODEOWNERS review required.
- Linear history required; force pushes rejected; stale approvals dismissed.
- No required status checks yet.

Planned follow-up: an aggregate `ci-gate` workflow that collects the outcome of
all check runs for the head commit and reports a single `ci-gate` check. Once
it exists, branch protection marks only `ci-gate` as required, which resolves
the paths-filter conflict. Tracked as a follow-up issue; this document records
the decision.

## 7. Repository variable switches

Feature gates are plain repository variables (`Settings > Secrets and
variables > Actions > Variables`), not secrets. All are off at Phase 0.5.

| Variable | Consumed by | Default | Effect when off | Flipped on in |
| --- | --- | --- | --- | --- |
| `ENABLE_HIL` | `hil.yml` | `false` | HIL jobs self-skip with a notice | Phase 1 (HIL runner online) |
| `ENABLE_NIGHTLY_SOAK` | `nightly.yml` | `false` | Long QEMU soak leg self-skips | Phase 2 (scenario suites stable) |
| `ENABLE_CLOUD_DEPLOY` | `deploy-cloud.yml` | `false` | Staging/production deploy jobs self-skip | Phase 4 (deploy-cloud with WIF + environments) |
| `ENABLE_RELEASE_PIPELINE` | `release.yml` | `false` | Release automation self-skips; only release-please changelog/version PRs run | Phase 6 (first tagged release) |
| `ENABLE_SBOM_CHECK` | `nightly.yml` | `false` | SBOM/NVD check leg self-skips | Phase 6 (SBOM delivered) |

## 8. Private vs public repository features

Several security features are plan- and visibility-gated. Where a feature is
unavailable, the job self-skips with a notice instead of failing.

| Capability | Private repo (no GitHub Advanced Security) | Public repo |
| --- | --- | --- |
| CodeQL | Unavailable; `codeql.yml` self-skips with a notice | Runs on PR + push + weekly |
| Dependency review | Unavailable; `dependency-review.yml` self-skips | Runs on every PR |
| Secret scanning + push protection | Unavailable; settings hidden | Enable in repo settings |
| Scorecard | Runs weekly; result publishing limited — VERIFY: confirm exact private-repo behavior of `ossf/scorecard-action` | Full publishing + badge |
| GitHub Pages | Not available on the Free plan; `pages.yml` self-skips | Available |

- Recommendation: flip the repository to public before Phase 6 — the web
  flasher requires GitHub Pages, and public visibility unlocks CodeQL,
  dependency review, secret scanning and full Scorecard at no cost.
- Secret scanning + push protection + private vulnerability reporting are
  enabled in repo settings wherever the plan allows (spec C6).
- Independently of visibility, fork PRs never run on the self-hosted HIL
  runner (spec C10/G4).

## 9. Dependabot coverage

| Ecosystem | Directory | Schedule |
| --- | --- | --- |
| `github-actions` | `/` | Weekly; grouped minor/patch; `chore(deps):` prefix; area labels |
| `pip` | `/firmware-main/test` | Weekly; keeps pytest-embedded at the 2.9.3 pin unless reviewed |
| `npm` | `/cloud` | Weekly; activated as `package.json` files land |

Spec C7 also lists `gradle /android-app` and the `cloud/functions` /
`cloud/bridge` npm directories; those entries are added as the packages gain
build files — until then `ci-android.yml` and `ci-cloud.yml` self-skip.

ESP-IDF component manager dependencies (`idf_component.yml`) are not a
Dependabot ecosystem; the nightly drift check covers them (Section 5).

## 10. Cloud deploy notes (WIF)

`deploy-cloud.yml` authenticates via `google-github-actions/auth` with
Workload Identity Federation (job permission `id-token: write`); no
service-account JSON keys exist. The attribute condition is pinned to this
repository and to `refs/heads/main` / release tags. The first step after auth
is `firebase projects:list` as an explicit auth check.

Known issue: firebase-tools v15 has reports of ignoring WIF ADC
(firebase-tools#10726). Workaround: the firebase-tools version is pinned in
`cloud/package.json` instead of floating. VERIFY: identify the newest
firebase-tools version that still honors WIF ADC and pin it; alternative
workarounds are tracked in firebase-tools#10726. Owner: Phase 4.

## 11. Consolidated VERIFY appendix

Project-wide register of unverified assumptions (spec G1). Each item is owned
by a phase and must be resolved (and this table updated) before that phase exits.

| # | VERIFY item | Documented in | Owner phase |
| --- | --- | --- | --- |
| 1 | `espsecure` v5.x HSM/PKCS#11 signing syntax must be confirmed before wiring the KMS/HSM signing path | `security-keys.md`, `release-process.md` | Phase 7 |
| 2 | ESP Web Tools SRI hash covers the entry module only; dynamically imported sibling files cannot be covered by a single script tag; consider vendoring the full `dist/web` directory | `web-flasher.md` | Phase 6 |
| 3 | firebase-tools WIF/ADC regression (firebase-tools#10726): pin a working version | `ci-cd.md` Section 10 | Phase 4 |
| 4 | `gradle/actions/setup-gradle` needs a SHA pin when `ci-android.yml` activates | `ci-cd.md` Section 4 | Phase 5 |
| 5 | Scorecard publishing behavior on private repositories | `ci-cd.md` Section 8 | Phase 7 |
| 6 | Secure boot v2 on ESP32-S3 under QEMU is unproven (expected: flash encryption works, SBv2 unknown) — Phase 0.5 spike (e) | `security-keys.md` | Phase 0.5 |
| 7 | RP2040 BOOTSEL drag-drop flashing procedure to be confirmed against Raspberry Pi official docs | `web-flasher.md` | Phase 3 |
| 8 | QEMU flash mode: sdkconfig.qemu uses DIO as a precaution; verify QIO works under QEMU — Phase 0.5 spike (a) | `qemu-testing.md` | Phase 0.5 |
| 9 | ESP32-S3 secure boot v2 key revocation slot budget must be confirmed before planning key rotation for hardened devices | `security-keys.md` | Phase 7 |
