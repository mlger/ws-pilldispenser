# ota (Phase 6)

OTA update component: HTTPS download, rollback with self-test marker
(`esp_ota_mark_app_valid_cancel_rollback`), version reporting. Tested under
QEMU with local HTTP servers (docs/test-plan.md ota suite). Production units
update via signed OTA only (docs/security-keys.md).
