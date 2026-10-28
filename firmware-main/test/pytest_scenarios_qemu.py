"""Scenario tests: full firmware shape with the hal_sim backend under QEMU.

Runs on: qemu. CI: ci-firmware-main -> qemu-tests.
Phase 0.5 ships the boot smoke test; the dose/DST/power-loss/offline scenarios
below are the Phase 2+ contract and are wired to real behavior as those
phases land (they stay visible as skipped tests so the plan cannot rot).
"""

import pytest

try:
    from pytest_embedded_idf import IdfDut
except ImportError:  # pragma: no cover - older plugin layouts
    from pytest_embedded_idf.dut import IdfDut  # type: ignore[no-redef]


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_boot_smoke(dut: IdfDut) -> None:
    """Scenario app boots and reports the deterministic ready marker."""
    dut.expect_exact('PD_READY', timeout=60)


@pytest.mark.qemu
@pytest.mark.skip(reason='Phase 2: requires core_scheduler wiring into the scenario app')
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_dose_due_across_eu_dst(dut: IdfDut) -> None:
    """EU DST spring-forward: dose due survives the 02:00 -> 03:00 jump."""
    dut.expect_exact('PD_READY', timeout=60)
    dut.write('sim tz CET-1CEST,M3.5.0,M10.5.0/3')
    dut.write('sim time set 2026-03-29T00:59:50Z')  # DST jump
    dut.write('sim coproc auto-ack on')
    dut.expect(r'FSM: DOSE_DUE slot=\d+', timeout=30)
    dut.expect_exact('FSM: DISPENSED', timeout=30)
    dut.expect_exact('MQTT: event published dose', timeout=30)


@pytest.mark.qemu
@pytest.mark.skip(reason='Phase 2: requires NVS/LittleFS dose journal')
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_power_loss_mid_dispense_replays_journal(dut: IdfDut) -> None:
    """Kill QEMU mid-dispense, restart on the same flash image, verify the NVS
    journal replays and no double dose is dispensed (FMEA-04)."""


@pytest.mark.qemu
@pytest.mark.skip(reason='Phase 4: requires sim_net (open_eth) + mosquitto fixture')
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_offline_queue_flushes_when_broker_returns(dut: IdfDut) -> None:
    """Events queued while the broker is down are published in order on
    reconnect (docs/test-plan.md mqtt suite)."""
