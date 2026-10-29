# Offline persistence: NVS dose journal (write-ahead), dose-slot config and
# per-slot last-fired-key table (spec Phase 2, FMEA-01/04). NVS is available on
# esp32s3 and the linux target; the host CTest build does not cover this
# component (QEMU Unity suite only).
idf_component_register(SRCS "src/storage.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash core_scheduler log)
