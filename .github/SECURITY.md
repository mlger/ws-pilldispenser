# Security Policy

## Reporting a vulnerability

Use **GitHub private vulnerability reporting** (Security tab -> Report a vulnerability).
Do not open a public issue for security problems.

Dose-safety issues (missed dose, double dose, wrong time) reported here are triaged **P0**.

## Scope

- `firmware-main` (ESP32-S3 main unit), `firmware-coproc` (mechanism controller)
- `cloud` (Firebase functions, bridge worker, Firestore rules, MQTT broker)
- `android-app`, `web-flasher` (GitHub Pages static site)

## Handling rules baked into this repository

- Secrets and signing keys never live in the repo or CI logs — see
  [docs/security-keys.md](docs/security-keys.md).
- CI and HIL runners never burn real eFuses (enforced by `tools/ci/check_no_efuse_burn.sh`).
- Production-hardened devices accept signed OTA updates only; the web flasher never
  offers hardened images.
