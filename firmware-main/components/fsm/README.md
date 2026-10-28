# fsm (Phase 2)

Application-level dispense FSM service wrapping `core_fsm`: event queue,
journal writes (NVS/LittleFS), coproc_link integration, alarm handling.
Pure logic lives in `core_fsm` (tested on host + QEMU).
