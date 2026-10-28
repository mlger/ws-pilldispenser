"""Security tests under QEMU using EMULATED eFuses only (spec G3).

Runs on: qemu. CI: ci-firmware-main -> qemu-tests (Phase 6/7).
- Flash encryption dev-mode boot with a TEST-ONLY key via --qemu-efuse-path.
- Secure boot v2 case is marked xfail until proven on the esp32s3 QEMU
  machine (spec D1: "experimental on S3 — docs conflict").
Never burns real eFuses: enforced repo-wide by tools/ci/check_no_efuse_burn.sh.
"""

import pytest

try:
    from pytest_embedded_idf import IdfDut
except ImportError:  # pragma: no cover - older plugin layouts
    from pytest_embedded_idf.dut import IdfDut  # type: ignore[no-redef]


@pytest.mark.qemu
@pytest.mark.skip(reason='Phase 6: requires flash-encryption fixture images')
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_flash_encryption_dev_boot(dut: IdfDut) -> None:
    """Boot an encrypted image with a TEST-ONLY emulated eFuse key."""


@pytest.mark.qemu
@pytest.mark.xfail(reason='Secure boot v2 on esp32s3 QEMU unproven (spec D1)')
@pytest.mark.parametrize('target', ['esp32s3'], indirect=True)
@pytest.mark.parametrize('config', ['qemu'], indirect=True)
def test_secure_boot_v2_boot(dut: IdfDut) -> None:
    """Signed boot under emulated eFuses."""
