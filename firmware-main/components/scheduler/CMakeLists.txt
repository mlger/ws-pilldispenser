# Scheduler service: 1 s tick task wiring core_scheduler recurrence math to the
# RTC HAL, with persisted slot configuration and per-slot dose-key dedup via
# the storage component (spec Phase 2). App-level (uses FreeRTOS + HAL); the
# pure math it calls lives in core_scheduler and is host-tested.
idf_component_register(SRCS "src/scheduler_svc.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core_scheduler pd_hal storage log freertos)
