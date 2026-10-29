# FSM service: owns the core_fsm context, maps PD-Link frames and scheduler
# dose-due callbacks to FSM events, executes journal/inventory/alarm actions
# and ACK/completion deadlines (spec Phase 2, docs/protocol.md section 7).
idf_component_register(SRCS "src/fsm_svc.c"
                       INCLUDE_DIRS "include"
                       REQUIRES core_fsm core_protocol core_inventory pd_hal storage log
                                esp_timer freertos)
