#include "fsm_svc.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "core_fsm.h"
#include "core_inventory.h"
#include "core_protocol.h"
#include "hal_coproc.h"
#include "storage.h"

static const char *TAG = "pd_fsm";

typedef struct {
    pd_fsm_event_t ev;
    uint8_t slot;
    uint64_t key;
} fsm_q_item_t;

typedef enum { RX_IDLE = 0, RX_WAIT_ACK, RX_WAIT_COMPLETE } rx_phase_t;

static struct {
    pd_fsm_ctx_t ctx;
    QueueHandle_t q;
    /* One deadline timer serves both the ACK deadline (500 ms) and the
     * completion deadline (10 s); it is re-armed on every phase change. */
    esp_timer_handle_t deadline_timer;
    pd_inventory_t inv;
    uint8_t seq;
    rx_phase_t rx_phase;
    uint8_t rx_buf[2 * PD_PROTO_MAX_FRAME];
    size_t rx_len;
    int64_t last_poll_us;
    /* one-deep dose deferred while a cycle is running */
    bool pending_valid;
    uint8_t pending_slot;
    uint64_t pending_key;
    /* current dose key (set with DOSE_DUE, logged with the journal/events) */
    uint64_t cur_key;
    bool power_loss_test;
    bool started;
} s_svc;

static void handle_event(pd_fsm_event_t ev, uint8_t slot, uint64_t key);
static void act_send_req(void);

static void post_event(pd_fsm_event_t ev, uint8_t slot, uint64_t key) {
    const fsm_q_item_t item = {.ev = ev, .slot = slot, .key = key};
    (void)xQueueSend(s_svc.q, &item, 0);
}

static void deadline_cb(void *arg) {
    (void)arg;
    post_event(PD_EV_TIMEOUT, 0, 0);
}

static void arm_deadline(uint64_t ms) {
    (void)esp_timer_stop(s_svc.deadline_timer);
    (void)esp_timer_start_once(s_svc.deadline_timer, ms * 1000ull);
}

static void stop_deadline(void) {
    (void)esp_timer_stop(s_svc.deadline_timer);
}

/* ------------------------------------------------------------------ */
/* Action execution                                                    */
/* ------------------------------------------------------------------ */

static void exec_actions(uint32_t actions, pd_fsm_state_t prev_state, pd_fsm_event_t cause) {
    const uint8_t slot = s_svc.ctx.slot_id;

    /* State-entry marker first: tests and operators read the transition, then
     * the actions it triggered (EVENT/ALARM/BUZZ follow in a fixed order). */
    if (s_svc.ctx.state != prev_state) {
        if (s_svc.ctx.state == PD_FSM_FAILED || s_svc.ctx.state == PD_FSM_FAULT) {
            ESP_LOGI(TAG, "FSM: %s slot=%u reason=%s", pd_fsm_state_name(s_svc.ctx.state),
                     (unsigned)slot, pd_fsm_event_name(cause));
        } else {
            ESP_LOGI(TAG, "FSM: %s slot=%u", pd_fsm_state_name(s_svc.ctx.state), (unsigned)slot);
        }
    }

    if (actions & PD_ACT_SEND_DISPENSE_REQ) {
        act_send_req();
    }
    if (actions & PD_ACT_MARK_JOURNAL) {
        (void)pd_storage_journal_commit();
        (void)pd_inv_take(&s_svc.inv, slot, 1);
        ESP_LOGI(TAG, "EVENT: dose published slot=%u key=%llu", (unsigned)slot,
                 (unsigned long long)s_svc.cur_key);
    }
    if (actions & PD_ACT_SIGNAL_USER) {
        ESP_LOGI(TAG, "BUZZ: signal user slot=%u", (unsigned)slot);
    }
    if (actions & PD_ACT_ALARM) {
        ESP_LOGI(TAG, "ALARM: %s slot=%u reason=%s", pd_fsm_state_name(s_svc.ctx.state),
                 (unsigned)slot, pd_fsm_event_name(cause));
    }
}

static void handle_event(pd_fsm_event_t ev, uint8_t slot, uint64_t key) {
    const pd_fsm_state_t prev = s_svc.ctx.state;

    if (ev == PD_EV_DOSE_DUE) {
        stop_deadline();
        s_svc.ctx.slot_id = slot;
        s_svc.cur_key = key;
    } else if (ev == PD_EV_START_ACK) {
        stop_deadline();
        arm_deadline(PD_FSM_SVC_COMPLETE_TIMEOUT_MS);
        s_svc.rx_phase = RX_WAIT_COMPLETE;
        s_svc.last_poll_us = 0; /* STATUS poll immediately */
    } else if (ev == PD_EV_COMPLETE || ev == PD_EV_JAM || ev == PD_EV_EMPTY ||
               ev == PD_EV_TIMEOUT) {
        stop_deadline();
        s_svc.rx_phase = RX_IDLE;
    }

    const uint32_t actions = pd_fsm_step(&s_svc.ctx, ev);
    exec_actions(actions, prev, ev);
}

/* ------------------------------------------------------------------ */
/* Coprocessor frame pump                                              */
/* ------------------------------------------------------------------ */

static void on_frame(const pd_frame_t *f) {
    ESP_LOGI(TAG, "PD_LINK: rx type=%02x seq=%02x len=%u", f->type, f->seq,
             (unsigned)f->payload_len);

    if (s_svc.rx_phase == RX_WAIT_ACK && f->type == PD_FRAME_ACK && f->payload_len >= 1 &&
        f->payload[0] == s_svc.seq) {
        handle_event(PD_EV_START_ACK, 0, 0);
        return;
    }
    if (s_svc.rx_phase == RX_WAIT_COMPLETE && f->type == PD_FRAME_STATUS_RESP &&
        f->payload_len >= 3) {
        const uint8_t state = f->payload[0];
        const uint8_t jam = f->payload[1];
        const uint8_t empty = f->payload[2];
        if (jam) {
            handle_event(PD_EV_JAM, 0, 0);
        } else if (empty) {
            handle_event(PD_EV_EMPTY, 0, 0);
        } else if (state == 0) {
            handle_event(PD_EV_COMPLETE, 0, 0);
        }
        /* state != 0: mechanism still busy; keep polling */
        return;
    }
    if (f->type == PD_FRAME_JAM_EVENT) {
        handle_event(PD_EV_JAM, 0, 0);
        return;
    }
    if (f->type == PD_FRAME_EMPTY_EVENT) {
        handle_event(PD_EV_EMPTY, 0, 0);
        return;
    }
    /* NACK maps into the retry budget for Phase 2 (full NACK semantics with a
     * direct INVALID_PARAM->FAILED edge land with the Phase 3 coproc_link
     * retry engine, docs/protocol.md section 6). */
    if (f->type == PD_FRAME_NACK && s_svc.rx_phase == RX_WAIT_ACK) {
        handle_event(PD_EV_TIMEOUT, 0, 0);
        return;
    }
}

static void pump_frames(void) {
    uint8_t chunk[PD_PROTO_MAX_FRAME];
    size_t len = 0;
    while (pd_hal_coproc_recv(chunk, sizeof(chunk), 0, &len) == PD_HAL_OK && len > 0) {
        if (s_svc.rx_len + len > sizeof(s_svc.rx_buf)) {
            s_svc.rx_len = 0; /* resync */
            continue;
        }
        memcpy(&s_svc.rx_buf[s_svc.rx_len], chunk, len);
        s_svc.rx_len += len;

        for (;;) {
            pd_frame_t f;
            const int rc = pd_proto_try_decode(s_svc.rx_buf, s_svc.rx_len, &f);
            if (rc > 0) {
                on_frame(&f);
                const size_t consumed = (size_t)rc;
                s_svc.rx_len -= consumed;
                memmove(s_svc.rx_buf, &s_svc.rx_buf[consumed], s_svc.rx_len);
                continue;
            }
            if (rc < 0) {
                /* Framing/CRC error: resynchronize to the next SOF byte. */
                size_t i = 1;
                while (i < s_svc.rx_len && s_svc.rx_buf[i] != PD_PROTO_SOF) {
                    i++;
                }
                s_svc.rx_len -= i;
                memmove(s_svc.rx_buf, &s_svc.rx_buf[i], s_svc.rx_len);
                continue;
            }
            break; /* need more bytes */
        }
        len = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Dispense request (write-ahead journal first, FMEA-01/04)            */
/* ------------------------------------------------------------------ */

static void act_send_req(void) {
    const uint8_t slot = s_svc.ctx.slot_id;

    if (s_svc.ctx.attempts > 1) {
        ESP_LOGI(TAG, "FSM: retry slot=%u attempt=%u", (unsigned)slot,
                 (unsigned)s_svc.ctx.attempts);
    }

    /* Write-ahead journal: the PENDING entry must be durable before the
     * request leaves the device. If the journal cannot be written, abort on
     * the safe side - no dose without a durable trace. */
    if (pd_storage_journal_begin(slot, s_svc.cur_key) != 0) {
        ESP_LOGE(TAG, "FSM: journal write failed slot=%u, aborting dispense", (unsigned)slot);
        handle_event(PD_EV_EMPTY, 0, 0); /* -> FAILED + alarm, no request sent */
        return;
    }

    if (s_svc.power_loss_test) {
        ESP_LOGI(TAG, "PWRTEST: journal pending, restarting now (power-loss emulation)");
        (void)fflush(stdout);
        esp_restart();
    }

    s_svc.seq = (uint8_t)(s_svc.seq + 1);
    pd_frame_t req = {0};
    req.type = PD_FRAME_DISPENSE_REQ;
    req.seq = s_svc.seq;
    req.payload_len = 2;
    req.payload[0] = slot;
    req.payload[1] = 1; /* pills per dispense (Phase 2) */

    uint8_t wire[PD_PROTO_MAX_FRAME];
    const size_t n = pd_proto_encode(&req, wire, sizeof(wire));
    if (n == 0) {
        ESP_LOGE(TAG, "FSM: encode failed slot=%u", (unsigned)slot);
        handle_event(PD_EV_EMPTY, 0, 0);
        return;
    }
    if (pd_hal_coproc_send(wire, n) != PD_HAL_OK) {
        ESP_LOGE(TAG, "FSM: coproc send failed slot=%u", (unsigned)slot);
        handle_event(PD_EV_TIMEOUT, 0, 0);
        return;
    }
    s_svc.rx_phase = RX_WAIT_ACK;
    s_svc.last_poll_us = 0;
    arm_deadline(PD_FSM_SVC_ACK_TIMEOUT_MS);
}

/* ------------------------------------------------------------------ */
/* Task                                                                */
/* ------------------------------------------------------------------ */

static void fsm_task(void *arg) {
    (void)arg;
    for (;;) {
        fsm_q_item_t item;
        if (xQueueReceive(s_svc.q, &item, pdMS_TO_TICKS(20)) == pdTRUE) {
            handle_event(item.ev, item.slot, item.key);
        }
        pump_frames();

        /* STATUS polling while DISPENSING (docs/protocol.md section 5). */
        if (s_svc.ctx.state == PD_FSM_DISPENSING) {
            const int64_t now_us = esp_timer_get_time();
            if (now_us - s_svc.last_poll_us >= PD_FSM_SVC_STATUS_POLL_MS * 1000ll) {
                s_svc.last_poll_us = now_us;
                pd_frame_t req = {0};
                req.type = PD_FRAME_STATUS_REQ;
                req.seq = s_svc.seq;
                uint8_t wire[PD_PROTO_MAX_FRAME];
                const size_t n = pd_proto_encode(&req, wire, sizeof(wire));
                if (n > 0) {
                    (void)pd_hal_coproc_send(wire, n);
                }
            }
        }

        /* Cycle finished: flush a dose that was deferred while busy. */
        if (s_svc.pending_valid &&
            (s_svc.ctx.state == PD_FSM_IDLE || s_svc.ctx.state == PD_FSM_DISPENSED)) {
            const fsm_q_item_t pending = {.ev = PD_EV_DOSE_DUE,
                                          .slot = s_svc.pending_slot,
                                          .key = s_svc.pending_key};
            s_svc.pending_valid = false;
            ESP_LOGI(TAG, "FSM: flushing deferred dose slot=%u", (unsigned)pending.slot);
            (void)xQueueSend(s_svc.q, &pending, 0);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int pd_fsm_svc_start(void) {
    if (s_svc.started) {
        return 0;
    }

    /* Boot journal replay (FMEA-04): a pending entry means the device lost
     * power mid-dispense. Resolve it as dispensed-unknown and persist its key
     * as fired so the scheduler never re-fires the same occurrence. */
    pd_journal_entry_t e;
    if (pd_storage_journal_pending(&e)) {
        ESP_LOGI(TAG, "JOURNAL: replay slot=%u key=%llu state=pending", (unsigned)e.slot_id,
                 (unsigned long long)e.dose_key);
        (void)pd_storage_journal_commit();
        (void)pd_storage_last_key_set(e.slot_id, e.dose_key);
        ESP_LOGI(TAG, "JOURNAL: resolved slot=%u action=no-redispense", (unsigned)e.slot_id);
    }

    memset(&s_svc.ctx, 0, sizeof(s_svc.ctx));
    s_svc.ctx.state = PD_FSM_IDLE;

    const esp_timer_create_args_t targs = {.callback = &deadline_cb,
                                           .arg = NULL,
                                           .dispatch_method = ESP_TIMER_TASK,
                                           .name = "pd_fsm_dl",
                                           .skip_unhandled_events = true};
    if (esp_timer_create(&targs, &s_svc.deadline_timer) != ESP_OK) {
        return -1;
    }

    s_svc.q = xQueueCreate(8, sizeof(fsm_q_item_t));
    if (s_svc.q == NULL) {
        return -1;
    }

    if (s_svc.inv.slot_count == 0) {
        (void)pd_fsm_svc_inventory_init(8);
    }

    if (xTaskCreate(fsm_task, "pd_fsm", PD_FSM_SVC_TASK_STACK, NULL, 5, NULL) != pdPASS) {
        return -1;
    }
    s_svc.started = true;
    ESP_LOGI(TAG, "FSM: service started");
    return 0;
}

int pd_fsm_svc_inventory_init(uint8_t slot_count) {
    if (!pd_inv_init(&s_svc.inv, slot_count)) {
        return -1;
    }
    for (uint8_t i = 0; i < s_svc.inv.slot_count; i++) {
        (void)pd_inv_load_slot(&s_svc.inv, s_svc.inv.slots[i].slot_id, 10, 10, "");
    }
    return 0;
}

int pd_fsm_svc_dose_due(uint8_t slot_id, uint64_t dose_key) {
    if (!s_svc.started || slot_id == 0) {
        return -1;
    }

    /* Service-level double-dose guard. core_fsm enforces the guard for IDLE
     * only; in DISPENSED (cycle complete, waiting for the next occurrence) a
     * same-key re-trigger must also be suppressed here - one wall-clock dose
     * key is one dose, regardless of the FSM state (FMEA-01). */
    const bool cycle_done = (s_svc.ctx.state == PD_FSM_DISPENSED) ||
                            (s_svc.ctx.state == PD_FSM_IDLE && s_svc.ctx.dispensed_flag);
    if (cycle_done && slot_id == s_svc.ctx.last_dispensed_slot && dose_key == s_svc.cur_key) {
        ESP_LOGI(TAG, "FSM: guard slot=%u key=%llu (duplicate dose suppressed)",
                 (unsigned)slot_id, (unsigned long long)dose_key);
        return 1;
    }

    if (s_svc.ctx.state != PD_FSM_IDLE && s_svc.ctx.state != PD_FSM_DISPENSED) {
        if (s_svc.pending_valid) {
            ESP_LOGW(TAG, "FSM: deferred slot=%u dropped (slot occupied)", (unsigned)slot_id);
            return -1;
        }
        s_svc.pending_valid = true;
        s_svc.pending_slot = slot_id;
        s_svc.pending_key = dose_key;
        ESP_LOGI(TAG, "FSM: deferred slot=%u key=%llu (busy)", (unsigned)slot_id,
                 (unsigned long long)dose_key);
        return 0;
    }

    post_event(PD_EV_DOSE_DUE, slot_id, dose_key);
    return 0;
}

int pd_fsm_svc_reset(void) {
    if (!s_svc.started) {
        return -1;
    }
    post_event(PD_EV_RESET, 0, 0);
    return 0;
}

int pd_fsm_svc_retry(void) {
    if (!s_svc.started) {
        return -1;
    }
    post_event(PD_EV_RETRY, 0, 0);
    return 0;
}

void pd_fsm_svc_arm_power_loss(void) {
    s_svc.power_loss_test = true;
}

const char *pd_fsm_svc_state_name(void) {
    return pd_fsm_state_name(s_svc.ctx.state);
}

uint8_t pd_fsm_svc_slot(void) {
    return s_svc.ctx.slot_id;
}

uint64_t pd_fsm_svc_key(void) {
    return s_svc.cur_key;
}

bool pd_fsm_svc_busy(void) {
    return s_svc.ctx.state == PD_FSM_DOSE_DUE || s_svc.ctx.state == PD_FSM_DISPENSING;
}
