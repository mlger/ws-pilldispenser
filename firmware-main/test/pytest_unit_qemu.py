"""Unity component tests under QEMU (scheduler, FSM, protocol codec, inventory).

Runs on: qemu. CI: ci-firmware-main -> qemu-tests.
The unit app prints the IDF unity menu; run_all_single_board_cases parses it
and executes every TEST_CASE on the esp32s3 machine.
"""

import pytest

try:
    from pytest_embedded_idf import IdfDut
except ImportError:  # pragma: no cover - older plugin layouts
    from pytest_embedded_idf.dut import IdfDut  # type: ignore[no-redef]


@pytest.mark.qemu
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_unity_components(dut: IdfDut) -> None:
    """Run every component TEST_CASE through the unity test menu."""
    dut.run_all_single_board_cases(timeout=120)
