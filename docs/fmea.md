# PillDispenser — Failure Mode and Effects Analysis (FMEA)

Status: Phase 0 living document; reviewed whenever a change affects dose timing or dose
count. Owner: @safety-owner.

## 1. Scope and method

This FMEA covers every component whose failure can affect **dose timing or dose count**:
`firmware-main` (scheduler, FSM, storage, comms, OTA), `firmware-coproc`, and — where
they gate a dose — the cloud bridge and Android app. Pure UI cosmetics and performance
regressions are out of scope.

Method: each failure mode is scored 1–10 for Severity (S, impact on the patient),
Occurrence (O, expected frequency given current controls) and Detection (D, likelihood
the CI/HIL stack catches it before release). `RPN = S x O x D`. Any row with
`RPN >= 100` **or** `S >= 9` requires a mitigation, a linked tracking issue and a
re-score at the next review. Ratings below are Phase 0 best estimates and are re-scored
whenever the document is reviewed.

### 1.1 Severity anchors (S)

| S | Meaning for this product |
| --- | --- |
| 9–10 | Overdose, tampered firmware altering dose behavior, or permanent loss of dose history |
| 6–8 | Missed or late dose, partial dose, patient confusion requiring caregiver intervention |
| 1–5 | Cosmetic, comfort, or recoverable inconvenience with no dose-content impact |

### 1.2 Occurrence anchors (O)

| O | Meaning |
| --- | --- |
| 1–2 | Only under compound failure; protected by redundant controls |
| 3–5 | Plausible per device-year given the current controls |
| 6–10 | Expected repeatedly without further mitigation |

### 1.3 Detection anchors (D)

| D | Meaning |
| --- | --- |
| 1–3 | Deterministically caught by host CTest or QEMU scenario suites on every PR |
| 4–6 | Caught by QEMU fault injection or HIL before release, but not on every PR |
| 7–10 | Only detectable in the field, by manual ceremony, or not detectable today |

## 2. Dose-integrity invariants

The mitigations in the worksheet rely on five invariants that the architecture enforces;
a PR that weakens any of them must update this FMEA:

1. **Journal write-ahead.** The dispense intent (occurrence id, slot, pill count) is
   committed to the NVS journal before `DISPENSE_REQ` is sent on RS485.
2. **One dispense per occurrence.** The FSM ignores a `DOSE_DUE` for an occurrence the
   journal already marks dispensed (double-dose guard).
3. **Idempotent transport.** A retransmitted `DISPENSE_REQ` (same `SEQ` within the retry
   window) is re-ACKed without repeating the mechanical action.
4. **Headless-safe dosing.** Scheduler, FSM, storage and alarms never depend on the
   display or touch being functional.
5. **No silent failure.** Every failed dose path ends in a user alarm and a telemetry
   event, never in an unrecorded state.

## 3. FMEA worksheet

| ID | Item | Potential failure mode | Local effect | End effect (patient) | S | O | D | RPN | Mitigation / control | How it is tested |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FMEA-01 | Dispense FSM + inventory | Same slot dispensed twice for one scheduled occurrence | Dose journal records two dispenses for one occurrence | Double dose — potential overdose | 10 | 3 | 3 | 90 | FSM double-dose guard (a `DOSE_DUE` for an already-dispensed occurrence is ignored); inventory journal write-ahead before actuation; protocol duplicate suppression (re-ACK of a retransmitted `DISPENSE_REQ` without re-actuating) | host (fsm/inventory unit tests); qemu (double-dose guard scenario); hil (end-to-end duplicate retry) |
| FMEA-02 | Scheduler | Scheduler never fires; dose is silently missed | No `DOSE_DUE` event; alarm escalation never triggers | Missed dose — therapy interruption | 8 | 3 | 4 | 96 | Scheduler unit tests over recurrence rules; alarm escalation ladder (buzzer, MQTT, app push); heartbeat telemetry exposes "no dose in N hours" | host (scheduler unit + DST edge tests); qemu (missed-dose scenario); hil (smoke) |
| FMEA-03 | Scheduler time math | Wrong dose time after timezone change or DST transition | Dose fires at shifted wall-clock time | Dose given hours early or late | 7 | 3 | 4 | 84 | POSIX TZ-string math centralized in `core_scheduler`; no local time arithmetic elsewhere; `sim tz` + `sim time set` inject EU and US spring/fall transitions | host (TZ math tests); qemu (DST scenario suite); hil (smoke) |
| FMEA-04 | Power domain | Power loss mid-dispense, before journal commit | Mechanism stops mid-cycle; RAM state lost | Partial or missing dose with no record — dose count corrupted | 9 | 4 | 4 | 144 | NVS journal write-ahead: intent is committed before `DISPENSE_REQ` is sent; on boot the journal is replayed and incomplete occurrences are flagged for caregiver action; coredump captured to the `coredump` partition | qemu (kill QEMU mid-dispense, restart on the same flash file, verify journal replay); hil (brown-out jig) |
| FMEA-05 | RTC | RTC drift accumulates beyond dose-window tolerance | Wall clock diverges from real time | Doses drift away from prescribed times | 6 | 4 | 5 | 120 | PCF85063 drift measured and budgeted; NTP reconciliation whenever the network is up; drift telemetry reported | hil (24 h drift measurement); qemu (`sim_rtc` offset injection exercises reconciliation logic); host (TZ/window math) |
| FMEA-06 | RTC supply | RTC backup battery failure; RTC time invalid after main power loss | Invalid timestamp at boot | Doses scheduled against a wrong clock | 7 | 3 | 5 | 105 | RTC invalid-flag check at boot (`hal_rtc`); on invalid time the device holds dispensing, alerts the user and reconciles from NTP before resuming | qemu (`sim_rtc` invalid-time injection); hil (battery removal test) |
| FMEA-07 | RS485 link | Frame corruption on the RS485 bus | Bad `SOF`/CRC frames; missing ACKs | Retried or failed dispense; delayed dose | 7 | 5 | 3 | 105 | CRC16-CCITT-FALSE on every frame; silent drop of bad frames + error counter; ACK within 500 ms with 3 retries / 250 ms backoff; FSM `TIMEOUT` escalates instead of guessing | qemu (`sim coproc fault crc`, `fault no-ack`); hil (real-link BER, CRC and timeout tests) |
| FMEA-08 | Co-processor | Co-processor hangs; stops responding | Requests time out; link dead | Dose not dispensed; alarm raised | 8 | 3 | 4 | 96 | Request timeout (500 ms x 3) maps to FSM `TIMEOUT` -> `FAILED` + user alarm; watchdog reset inside co-processor firmware; `HEARTBEAT` supervision detects a dead link. VERIFY: whether the main MCU also gets a hardware reset line to the co-processor is decided in Phase 3 | qemu (`sim coproc fault no-ack`, `fault timeout`); hil (jig-induced hang) |
| FMEA-09 | Dispense mechanism | Motor jam during dispense | Mechanism stalls; `JAM_EVENT` | Patient receives a partial dose or nothing | 8 | 4 | 3 | 96 | `JAM_EVENT` (or `jam=1` in `STATUS_RESP`) drives FSM `JAM` -> FAULT + user alarm; occurrence marked undispensed in the journal so it can be re-dosed safely; jam latching prevents retry storms | qemu (`sim coproc fault jam`); hil (mechanical jam jig) |
| FMEA-10 | Pill supply | Hopper/slot runs empty | `EMPTY_EVENT`; dispense aborts | Missed dose until refill | 5 | 5 | 3 | 75 | `EMPTY_EVENT` (or `empty=1`) drives FSM `EMPTY` -> refill flow; inventory tracks pill counts per slot; low-stock warning before empty | qemu (`sim coproc fault empty`); hil (empty-slot test) |
| FMEA-11 | UI hardware | Display or touch failure | Screen blank or unresponsive | User cannot interact, but doses still fire on schedule | 4 | 4 | 4 | 64 | Headless-safe design: scheduler/FSM/storage never depend on the UI; buzzer alarm path is independent of the display | qemu (UI-less scenario runs with sim stubs); hil (GT911 touch contract test, display render check) |
| FMEA-12 | OTA | OTA update bricks the device (bad image accepted) | Device fails self-test after update | Device offline; caregiver must visit to recover | 9 | 2 | 4 | 72 | Rollback via `otadata` + app validity marker: the bootloader reverts to the previous slot unless the new image passes self-test and calls the mark-valid API; dev-channel images for the web flasher, signed images for production | qemu (OTA suite: failed self-test -> rollback; passing self-test -> mark valid); hil (OTA on real flash) |
| FMEA-13 | Flash storage | Flash corruption (wear, brown-out during write) | NVS/LittleFS data damaged | Lost configuration or journal; wrong schedule after reboot | 7 | 2 | 5 | 70 | LittleFS journal with checksummed records; NVS keys schema-validated on load; invalid records quarantined, defaults restored, user alerted; `coredump` partition preserves evidence | qemu (power-loss replay suite over the journal); hil (reboot-under-write test) |
| FMEA-14 | Security | Secure boot / flash-encryption key leakage | Attacker can sign and flash malicious firmware | Tampered firmware could alter dose behavior | 10 | 1 | 7 | 70 | Secure boot v2 + flash encryption in release mode (Phase 7 ceremony); signing key held in KMS/HSM or the `firmware-signing` environment only, never in repo/artifacts/logs; offline key backup per `docs/security-keys.md`; eFuse burns barred from CI by `tools/ci/check_no_efuse_burn.sh` | qemu (flash encryption with emulated eFuses; secure boot v2 experimental on S3); manual two-person ceremony (not a CI test) |

## 4. Risk summary

Rows meeting the action threshold (`RPN >= 100` or `S >= 9`): FMEA-01, FMEA-04, FMEA-05,
FMEA-06, FMEA-07, FMEA-12, FMEA-14. Each requires a linked tracking issue and a re-score
at the next review. FMEA-04 is the highest combined risk (RPN 144) and drives the
write-ahead journal design. FMEA-01, FMEA-12 and FMEA-14 are severity-9+ rows: they are
triaged P0 per the `dose_safety_issue.yml` issue template even when RPN is below 100,
because severity dominates for dose-integrity and security failures.

## 5. Test-environment legend

| Tag | Environment | What it proves |
| --- | --- | --- |
| host | Pure C11 build, CTest with ASan/UBSan (`firmware-main/host_test`) | Business logic: scheduler, FSM, codec, inventory — deterministic, runs on every PR |
| qemu | Espressif QEMU (esp32s3) with `hal_sim`, pytest-embedded, fault injection via `sim_coproc`/`sim_rtc` | Firmware-level behavior: persistence, OTA rollback, DST jumps, transport faults |
| hil | Self-hosted runner `hil-4.3b` (gated by `ENABLE_HIL`), real board + co-processor + jig | Real hardware: I2C devices, RS485 link, SD card, RGB timing, drift, jam/empty jig |
| manual | Documented two-person ceremony | Only for secure boot v2 / flash encryption release provisioning (Phase 7) |

## 6. Change control

- This file is owned by @safety-owner in CODEOWNERS; changes require their review.
- The PR template asks "Safety impact (dose timing/count?) — FMEA updated? y/n" for
  changes affecting dose logic; a "yes" without a corresponding FMEA update blocks merge.
- New failure modes found during HIL, field triage, or a `dose_safety_issue.yml` report
  are added to the worksheet before the fixing PR merges.
- Rows whose mitigation changes (or whose S/O/D estimate changes) are re-scored in the
  same PR, and the sign-off table below gets a new row after review.

## 7. Review & sign-off

| Date | Reviewer | Result |
| --- | --- | --- |
| (placeholder — Phase 0 review pending) | @safety-owner | |
| | | |
| | | |
