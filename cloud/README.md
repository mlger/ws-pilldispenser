# cloud

Firebase backend (Phase 4, spec Section F): bridge worker (MQTT broker ↔
Firestore), Cloud Functions, `firestore.rules`, broker configuration.

## Planned shape

```
cloud/
├── bridge/         MQTT → Firestore bridge worker (device tokens, LWT)
├── functions/      Cloud Functions (dose events, refill logic)
├── firestore.rules Cross-household isolation, caregiver vs patient roles,
│                   write-once dose history
├── firebase.json
└── broker/         Mosquitto / EMQX configuration (production MQTT)
```

## Testing (ci-cloud)

- Firestore rules run under the emulator with `@firebase/rules-unit-testing`,
  covering cross-household isolation, caregiver vs patient roles, device
  tokens, and write-once dose history (spec C4).
- Bridge integration tests run against a local mosquitto with the throwaway
  test CA from `firmware-main/test/fixtures/gen_test_certs.sh` — the same
  fixtures the QEMU MQTT suite uses.

## Deploys (deploy-cloud)

Workload Identity Federation only — no service-account JSON keys.
Environments: `staging` (auto on main), `production` (2 required reviewers
from /cloud CODEOWNERS). Gated by `vars.ENABLE_CLOUD_DEPLOY`.
