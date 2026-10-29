"""Scenario tests: full firmware shape with the hal_sim backend under QEMU.

Runs on: qemu. CI: ci-firmware-main -> qemu-tests.
Phase 2 suite: scheduler -> FSM -> storage-journal dose pipeline, DST edges
(EU + US), fall-back overlap dedup, missed-dose catch-up, double-dose guard,
fault injection (jam/empty/no-ack) and power-loss journal replay.

Marker contract (all logged by the firmware, docs/qemu-testing.md):
  PD_READY                                 all services up
  SCHED: due slot=N key=K                  scheduler fired an occurrence
  SCHED: dedup slot=N key=K                occurrence suppressed (already fired)
  FSM: <STATE> slot=N [reason=E]           FSM state entry
  FSM: guard slot=N key=K                  service-level double-dose guard
  EVENT: dose published slot=N key=K       dose event emitted (MQTT lands Phase 4)
  JOURNAL: replay slot=N key=K state=pending   boot replay of a pending entry
  JOURNAL: resolved slot=N action=no-redispense
  PWRTEST: journal pending, restarting now     esp_restart mid-dispense
"""

import pytest

try:
    from pytest_embedded_idf import IdfDut
except ImportError:  # pragma: no cover - older plugin layouts
    from pytest_embedded_idf.dut import IdfDut  # type: ignore[no-redef]

CMD_TIMEOUT = 60
DOSE_TIMEOUT = 30


def _console_ready(dut: IdfDut) -> None:
    """Wait for the REPL to drain input. Commands written before this marker
    are lost in the QEMU UART FIFO (console task not yet running)."""
    dut.expect_exact('SIM: console ready', timeout=CMD_TIMEOUT)


def _arm(dut: IdfDut, tz: str, hhmm: str, when: str) -> None:
    """Common scenario arming: timezone, slot 1, auto-ack, wall clock.

    Call after the test consumed PD_READY; this waits for the REPL to be
    draining input and then writes the four setup commands."""
    _console_ready(dut)
    dut.write(f'sim tz {tz}')
    dut.write(f'pd slots set 1 {hhmm} daily')
    dut.write('sim coproc auto-ack on')
    dut.write(f'sim time set {when}')


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_boot_smoke(dut: IdfDut) -> None:
    """Scenario app boots and reports the deterministic ready marker."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _console_ready(dut)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_dose_due_across_eu_dst_spring(dut: IdfDut) -> None:
    """EU DST spring-forward 2026-03-29 (02:00 -> 03:00 CET->CEST): the dose at
    03:00 local is due exactly at the transition instant; the skipped wall hour
    must not produce a spurious dose and the dose fires once."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'CET-1CEST,M3.5.0,M10.5.0/3', '03:00', '2026-03-29T01:00:00Z')
    dut.expect_exact('FSM: DOSE_DUE slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSED slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('EVENT: dose published slot=1 key=202603290300', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_eu_dst_fall_back_no_double_dose(dut: IdfDut) -> None:
    """EU DST fall-back 2026-10-25 (03:00 -> 02:00 CEST->CET): wall clock
    02:30 happens twice; the second pass must be suppressed by the persisted
    dose-key dedup (FMEA-04), not dispensed twice."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'CET-1CEST,M3.5.0,M10.5.0/3', '02:30', '2026-10-25T00:30:00Z')
    dut.expect_exact('FSM: DOSE_DUE slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSED slot=1', timeout=DOSE_TIMEOUT)

    # One real hour later the wall clock shows 02:30 again (CET pass).
    dut.write('sim time set 2026-10-25T01:30:00Z')
    dut.expect_exact('SCHED: dedup slot=1 key=202610250230', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_us_dst_spring(dut: IdfDut) -> None:
    """US DST spring-forward 2026-03-08 (02:00 -> 03:00 EST->EDT): the dose at
    03:00 local fires at the transition instant, exactly once."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'EST5EDT,M3.2.0,M11.1.0', '03:00', '2026-03-08T07:00:00Z')
    dut.expect_exact('FSM: DOSE_DUE slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSED slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('EVENT: dose published slot=1 key=202603080300', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_us_dst_fall_back_no_double_dose(dut: IdfDut) -> None:
    """US DST fall-back 2026-11-01 (02:00 -> 01:00 EDT->EST): wall clock 01:30
    happens twice; one dose, second pass deduped."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'EST5EDT,M3.2.0,M11.1.0', '01:30', '2026-11-01T05:30:00Z')
    dut.expect_exact('FSM: DOSE_DUE slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSED slot=1', timeout=DOSE_TIMEOUT)

    dut.write('sim time set 2026-11-01T06:30:00Z')
    dut.expect_exact('SCHED: dedup slot=1 key=202611010130', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_leap_day_dose_and_next_due(dut: IdfDut) -> None:
    """Leap day 2028-02-29: the dose fires on the leap day and the next daily
    occurrence rolls to 2028-03-01."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'UTC0', '09:00', '2028-02-29T09:00:30Z')
    dut.expect_exact('FSM: DOSE_DUE slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSED slot=1', timeout=DOSE_TIMEOUT)

    dut.write('pd status')
    dut.expect_exact('PD: slot=1 09:00 rec=1 en=1 next=2028-03-01T09:00:00', timeout=CMD_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_missed_dose_catchup(dut: IdfDut) -> None:
    """A dose whose scheduled time passed while the clock jumped forward is
    caught up late (within the catch-up window) exactly once."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'UTC0', '08:00', '2026-01-15T07:00:00Z')

    # No dose yet at 07:00 (the 08:00 occurrence is still in the future).
    dut.write('pd status')
    dut.expect_exact('PD: journal=none', timeout=CMD_TIMEOUT)

    # Jump past the dose time: the catch-up window fires it late, once.
    dut.write('sim time set 2026-01-15T09:30:00Z')
    dut.expect_exact('SCHED: due slot=1 key=202601150800', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSED slot=1', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_double_dose_guard(dut: IdfDut) -> None:
    """After a completed dose, a re-trigger of the same wall-clock occurrence
    is suppressed by the guard; a different slot still dispenses."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'UTC0', '08:00', '2026-01-15T08:00:30Z')
    dut.expect_exact('FSM: DISPENSED slot=1', timeout=DOSE_TIMEOUT)

    # Same wall-minute occurrence for slot 1: guard suppresses, no new cycle.
    dut.write('pd dose trigger 1')
    dut.expect_exact('FSM: guard slot=1 key=202601150800', timeout=DOSE_TIMEOUT)

    # Slot 2 has its own key: a new cycle starts and completes.
    dut.write('pd slots set 2 08:00 daily')
    dut.write('pd dose trigger 2')
    dut.expect_exact('FSM: DOSE_DUE slot=2', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSED slot=2', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_jam_faults_to_fault_state(dut: IdfDut) -> None:
    """sim coproc fault jam: STATUS_RESP jam=1 during DISPENSING moves the FSM
    to FAULT with the alarm marker."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'UTC0', '08:00', '2026-01-15T08:00:30Z')
    dut.write('sim coproc fault jam')
    dut.expect_exact('FSM: DOSE_DUE slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSING slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: FAULT slot=1 reason=JAM', timeout=DOSE_TIMEOUT)
    dut.expect_exact('ALARM: FAULT slot=1 reason=JAM', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_empty_fails_with_alarm(dut: IdfDut) -> None:
    """sim coproc fault empty: STATUS_RESP empty=1 during DISPENSING moves the
    FSM to FAILED with the alarm marker."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'UTC0', '08:00', '2026-01-15T08:00:30Z')
    dut.write('sim coproc fault empty')
    dut.expect_exact('FSM: DOSE_DUE slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: DISPENSING slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: FAILED slot=1 reason=EMPTY', timeout=DOSE_TIMEOUT)
    dut.expect_exact('ALARM: FAILED slot=1 reason=EMPTY', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_no_ack_retries_then_failed(dut: IdfDut) -> None:
    """sim coproc fault no-ack: no response within the ACK deadline, three
    attempts total, then FSM FAILED with the timeout reason."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'UTC0', '08:00', '2026-01-15T08:00:30Z')
    dut.write('sim coproc fault no-ack')
    dut.expect_exact('FSM: DOSE_DUE slot=1', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: retry slot=1 attempt=2', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: retry slot=1 attempt=3', timeout=DOSE_TIMEOUT)
    dut.expect_exact('FSM: FAILED slot=1 reason=TIMEOUT', timeout=DOSE_TIMEOUT)
    dut.expect_exact('ALARM: FAILED slot=1 reason=TIMEOUT', timeout=DOSE_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_power_loss_mid_dispense_replays_journal(dut: IdfDut) -> None:
    """Power loss mid-dispense (journal PENDING committed, request never sent,
    then esp_restart): on boot the journal replays, is resolved as
    dispensed-unknown, the same wall-clock occurrence is never re-dispensed
    (scheduler dedup) and the journal is clean afterwards (FMEA-04)."""
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _arm(dut, 'UTC0', '08:00', '2026-01-15T08:00:30Z')

    dut.write('pd dose reboot 1')
    dut.expect_exact('PWRTEST: journal pending, restarting now', timeout=DOSE_TIMEOUT)

    # The device reboots into the same wall clock: replay + resolve, no
    # re-dispense of the pending occurrence.
    dut.expect_exact('JOURNAL: replay slot=1 key=202601150800 state=pending', timeout=CMD_TIMEOUT)
    dut.expect_exact('JOURNAL: resolved slot=1 action=no-redispense', timeout=CMD_TIMEOUT)
    dut.expect_exact('PD_READY', timeout=CMD_TIMEOUT)
    _console_ready(dut)

    # The scheduler must suppress the same occurrence (already fired).
    dut.expect_exact('SCHED: dedup slot=1 key=202601150800', timeout=DOSE_TIMEOUT)

    # And the journal is clean again.
    dut.write('pd status')
    dut.expect_exact('PD: journal=none', timeout=CMD_TIMEOUT)


@pytest.mark.qemu
@pytest.mark.skip(reason='Phase 4: requires sim_net (open_eth) + mosquitto fixture')
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
def test_offline_queue_flushes_when_broker_returns(dut: IdfDut) -> None:
    """Events queued while the broker is down are published in order on
    reconnect (docs/test-plan.md mqtt suite)."""
