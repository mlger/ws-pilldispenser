# scheduler (Phase 2)

Application-level scheduler service wrapping `core_scheduler`: dose slot
storage, due-tick task, missed-dose escalation, RTC/NTP reconciliation.
Pure logic lives in `core_scheduler`; this component owns FreeRTOS glue.
