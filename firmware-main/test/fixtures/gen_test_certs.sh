#!/usr/bin/env bash
# Generate a THROWAWAY test CA + server cert for the QEMU MQTT suite.
# TEST-ONLY: these credentials are worthless by construction and are never
# committed (gitignored). Regenerate freely; production certs are provisioned
# at the bench (docs/security-keys.md).
set -euo pipefail

out_dir="${1:-$(dirname "$0")/test-ca}"
mkdir -p "$out_dir"

openssl req -x509 -newkey rsa:2048 -nodes -days 30 \
    -keyout "$out_dir/ca.key" -out "$out_dir/ca.crt" \
    -subj "/CN=pd-test-ca" >/dev/null 2>&1

openssl req -newkey rsa:2048 -nodes \
    -keyout "$out_dir/server.key" -out "$out_dir/server.csr" \
    -subj "/CN=10.0.2.2" >/dev/null 2>&1

openssl x509 -req -in "$out_dir/server.csr" \
    -CA "$out_dir/ca.crt" -CAkey "$out_dir/ca.key" -CAcreateserial \
    -days 30 -out "$out_dir/server.crt" >/dev/null 2>&1

rm -f "$out_dir/server.csr"
echo "test CA written to $out_dir (TEST-ONLY)"
