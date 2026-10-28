#!/usr/bin/env bash
# Fails CI if any file attempts to burn REAL eFuses (spec G3).
# Allowed: QEMU-emulated eFuse usage (`idf.py qemu efuse-*`,
# `--qemu-efuse-path`, "emulated eFuse") in scripts, workflows, tests, docs.
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"

# Broad-catch patterns for real eFuse burns. docs/ is excluded: policy
# documents legitimately quote the forbidden commands. This script excludes
# itself for the same reason.
pattern='espefuse\.py[[:space:]]+burn|idf\.py[[:space:]]+efuse-burn|burn_efuse|efuse-burn-key|write_protect_efuse'

hits="$(grep -RInE --exclude-dir=.git --exclude-dir=build --exclude-dir=build* --exclude-dir=managed_components \
    --exclude-dir=docs \
    --exclude=check_no_efuse_burn.sh \
    "$pattern" "$root" 2>/dev/null || true)"

# Filter out sanctioned QEMU-emulated usage on the same line.
filtered=""
while IFS= read -r line; do
    [ -z "$line" ] && continue
    if echo "$line" | grep -qE 'qemu efuse|--qemu-efuse-path|emulated eFuse|emulated eFuses|qemu-efuse'; then
        continue
    fi
    filtered="${filtered}${line}\n"
done <<EOF
${hits}
EOF

if [ -n "$filtered" ]; then
    echo "ERROR: possible REAL eFuse burn detected (spec G3 forbids this outside QEMU):"
    printf '%b' "$filtered"
    exit 1
fi

echo "check_no_efuse_burn: OK (no real eFuse burn patterns found)"
