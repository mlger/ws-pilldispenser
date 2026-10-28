#!/usr/bin/env bash
# Sign bootloader + app images with the secure boot v2 key (spec C8/G2).
#
# DANGER: runs ONLY inside the `firmware-signing` GitHub environment, only for
# tags matching v*, with the key fetched via KMS/HSM+OIDC (preferred) or an
# environment secret decoded to tmpfs, used once, shredded. Never logs key
# material. See docs/security-keys.md.
set -euo pipefail

: "${SECURE_BOOT_KEY_FILE:?SECURE_BOOT_KEY_FILE must point at the signing key (tmpfs)}"
: "${BUILD_DIR:?BUILD_DIR must point at the release build directory}"

espsecure_version="$(espsecure.py version 2>/dev/null || espsecure version 2>/dev/null || echo unknown)"
echo "signing with espsecure ${espsecure_version} (key never printed)"

images=()
[ -f "${BUILD_DIR}/bootloader/bootloader.bin" ] && images+=("${BUILD_DIR}/bootloader/bootloader.bin")
[ -f "${BUILD_DIR}/pill_dispenser_main.bin" ] && images+=("${BUILD_DIR}/pill_dispenser_main.bin")

for img in "${images[@]}"; do
    out="${img%.bin}-prod-signed.bin"
    # VERIFY: confirm espsecure v5.x HSM/PKCS#11 invocation syntax before wiring
    # the KMS path (docs/ci-cd.md VERIFY appendix).
    espsecure.py sign_data --version 2 --key "${SECURE_BOOT_KEY_FILE}" --output "$out" "$img"
    echo "signed: $out"
done

# shred the tmpfs key copy after use
if [ -f "${SECURE_BOOT_KEY_FILE}" ]; then
    shred -u "${SECURE_BOOT_KEY_FILE}"
fi
echo "signing complete; key material removed"
