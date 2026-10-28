# Security, Keys and Provisioning

Status: Phase 0 living document. Owner: @repo-admin. Never contains real keys, passwords, or project IDs.

This document is the single reference for key material, secrets policy, eFuse
hard rules, the release-mode provisioning ceremony and incident response
(master-spec G2/G3). It must never contain real key material, passwords, or
real Firebase project IDs — placeholders and `vars`/`secrets` names only.

## 1. Key inventory

| Key | Where generated | Where stored | Where used | Rotation |
| --- | --- | --- | --- | --- |
| Secure boot v2 signing key | Offline, on an HSM/KMS or an air-gapped machine; never in the repo, never in an artifact, never printed | KMS/HSM (preferred) or `firmware-signing` environment secret (fallback); offline sealed backup held by two people | `release.yml` `sign` job via OIDC (espsecure); manual provisioning ceremony | No routine rotation; replacement requires re-provisioning every hardened device (Section 5) |
| Flash encryption key (production) | On the device itself, first boot in release mode | eFuse only; never exists outside the chip, never in CI | Device-local flash decryption at boot | None; bound to the device for its life |
| Flash encryption key (test) | Host-generated for QEMU tests | `test/fixtures`, marked TEST-ONLY | QEMU security tests with emulated eFuses | Disposable; regenerate any time |
| Android upload keystore | Generated once by the app owner | `android-release` environment secrets; Google Play holds the app signing key (Play App Signing enabled) | `release.yml` Android signing | Upload-key reset via Play Console; app signing key managed by Google |
| Firebase/GCP access | Not a key: Workload Identity Federation configuration | GCP IAM (identity pool + provider; attribute condition pinned to this repository) | `deploy-cloud.yml` via `google-github-actions/auth`, job permission `id-token: write` | No user-managed secret; provider managed in GCP |
| MQTT device certificates | Provisioned at the bench | Device NVS + broker trust store | Device-to-broker TLS | Per device, at the bench |
| MQTT test CA | Generated for the test suite | `test/fixtures` (throwaway, TEST-ONLY) | QEMU + HIL MQTT tests, local mosquitto | Disposable |
| `firmware-signing` fallback secret | Base64 copy of the signing key | GitHub environment secret | `sign` job fallback only: decoded to tmpfs, used once, shredded | Same policy as the signing key; delete if the KMS/HSM path is in use |

## 2. Signing and secrets policy (G2)

- The secure boot v2 key is never committed, never attached to artifacts,
  never echoed to logs. Preferred path: KMS/HSM-held key accessed from the
  `sign` job via OIDC (`id-token: write`) with remote signing (espsecure
  supports HSM/PKCS#11 signing — VERIFY: confirm espsecure v5.x
  HSM/PKCS#11 syntax before Phase 7).
- Fallback: base64 key in the `firmware-signing` environment secret, decoded
  to a tmpfs file, used once, shredded. The job is restricted to tags `v*`
  and requires reviewers.
- Offline backup of the signing key: two people, sealed. A lost key means no
  further OTA for hardened devices — this is the consequence of secure boot
  v2, not a process preference.
- Flash encryption: production devices generate their own key on first boot
  (release mode); host-generated keys exist only for QEMU tests in TEST-ONLY
  fixtures and never in CI.
- Android: upload keystore + passwords live only in `android-release`
  environment secrets; Play App Signing is enabled.
- Firebase/GCP: Workload Identity Federation only, no service-account JSON
  keys; the provider attribute condition is pinned to this repository.
- MQTT/TLS: device certificates are provisioned at the bench, never in CI;
  CI uses a throwaway test CA under `test/fixtures`.
- CI never holds any production credential. Environments
  (`firmware-signing`, `android-release`, `production`) require reviewers;
  `github-pages` and `staging` do not.

## 3. eFuse hard rules (G3)

1. CI and HIL runners must never burn real eFuses.
2. `espefuse.py burn_*` and `idf.py efuse-burn*` are allowed only as
   `idf.py qemu efuse-*` or with `--qemu-efuse-path` inside tests.
3. `tools/ci/check_no_efuse_burn.sh` enforces rule 2 on every PR; a violation
   fails the `lint` job.
4. HIL devices stay in development mode at all times: no secure boot, no
   flash encryption release mode.
5. Release-mode provisioning is a manual, two-person ceremony (Section 4)
   with a per-device log.
6. `tools/ci/check_release_sdkconfig.py` verifies the release sdkconfig:
   secure boot v2 on, flash encryption release mode on, JTAG/ROM download
   policy as designed, `PD_HAL_REAL=y`, `PD_SIM_CONSOLE=n`.

## 4. Release-mode provisioning ceremony

Performed per device, by two people (operator + witness), with a printed or
digitally filed log entry per device. The key material never appears in the
log — only digests.

1. Book a bench slot; confirm operator + witness are present; open a fresh
   log entry (template below).
2. Verify the device is a new or factory-reset unit; record device S/N and
   MAC.
3. Flash the release build; verify the sdkconfig hash against the release
   artifact using `tools/ci/check_release_sdkconfig.py`; attach its output.
4. Provision flash encryption in release mode; the device generates its own
   FE key on first boot; record the key digest on read-back (never the key).
5. Provision secure boot v2: sign the bootloader with the signing key and
   burn the public key digest. The signing key itself is never written to or
   near the device.
6. Verify: eFuse summary read-back matches expectations; encrypted boot
   smoke-test passes; a signed OTA from the beta channel applies; a
   plaintext/unsigned image is rejected.
7. Operator and witness both sign the log; file it (offline + repository-
   external storage). Any failed step quarantines the device — it does not
   ship and it does not re-enter the dev pool.

Log template (one per device; digests only, never keys):

```text
# PillDispenser provisioning log — hardened unit
date:             YYYY-MM-DD HH:MM (timezone)
device S/N:       PD-________
MAC (eFuse):      __:__:__:__:__:__
firmware:         pd-main <version> (release tag <tag>)
sdkconfig hash:   sha256:________   (verified by tools/ci/check_release_sdkconfig.py)
SBv2 key digest:  sha256:________   (public key digest burned; key never on device)
FE key:           generated on device (first boot); digest: sha256:________
OTA check:        signed OTA <version> applied OK / unsigned image rejected OK
performed-by:     operator ____________  witness ____________
notes:            __________________________________________
```

## 5. Key rotation note

Secure boot v2 anchors trust in the key digest burned into eFuses, so
replacing the signing key does not update existing devices. Rotation planning
must account for spare key slots on the ESP32-S3 — VERIFY: confirm the
ESP32-S3 secure boot v2 key-revocation slot budget before designing a
rotation scheme. Until then, treat the signing key as effectively permanent
and protect it accordingly.

## 6. Incident response

- **Compromised signing key**: (1) stop releases — revoke the key in
  KMS/HSM and remove the fallback environment secret; (2) generate a new
  key and re-run the provisioning ceremony for new devices; (3) devices
  provisioned with the compromised key's digest accept only old-key images
  and must be re-provisioned at the bench or retired; (4) audit release
  artifacts for anything signed in the exposure window.
- **Leaked CI secret**: revoke first, then investigate. Revoke/rotate the
  credential before forensic work; audit usage logs for the exposure window;
  then determine scope and fix the leak path.
- **Lost signing key**: consequence statement — if both the live key and the
  offline sealed backup are lost, no further OTA is possible for hardened
  devices; they keep running the last signed image but can never be updated
  again. This is why the backup is two-person sealed and stored offline.
- **Leaked test CA or TEST-ONLY fixtures**: regenerate; they never grant
  access to production systems.
