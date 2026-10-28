/*
 * sim_coproc — in-process co-processor model speaking real PD-Link v1 frames
 * (spec A1). pd_hal_coproc_send decodes the transmitted frame and, with
 * auto-ack on, enqueues the response a real mechanism would (ACK with matching
 * SEQ). Fault injection (`sim coproc fault <type>`) covers the FMEA cases:
 * CRC corruption, total timeout, jam and empty events. The FSM suite drives
 * this from QEMU in Phase 2/3; the codec itself is core_protocol.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "core_protocol.h"

#include "hal_coproc.h"

#include "sim_private.h"

static const char *TAG = "sim_coproc";

#define SIM_COPROC_QUEUE_LEN 16

static struct {
    uint8_t bytes[PD_PROTO_MAX_FRAME];
    size_t len;
} s_queue[SIM_COPROC_QUEUE_LEN];
static size_t s_head;
static size_t s_count;

static bool s_auto_ack;
static sim_coproc_fault_t s_fault;

void sim_coproc_set_auto_ack(bool on) {
    s_auto_ack = on;
    ESP_LOGI(TAG, "SIM_COPROC: auto-ack=%s", on ? "on" : "off");
}

void sim_coproc_set_fault(sim_coproc_fault_t fault) {
    s_fault = fault;
    ESP_LOGI(TAG, "SIM_COPROC: fault=%s", sim_coproc_fault_name(fault));
}

const char *sim_coproc_fault_name(sim_coproc_fault_t fault) {
    switch (fault) {
    case SIM_COPROC_FAULT_CRC:
        return "crc";
    case SIM_COPROC_FAULT_TIMEOUT:
        return "timeout";
    case SIM_COPROC_FAULT_JAM:
        return "jam";
    case SIM_COPROC_FAULT_EMPTY:
        return "empty";
    case SIM_COPROC_FAULT_NONE:
    default:
        return "none";
    }
}

static bool coproc_enqueue(const pd_frame_t *frame) {
    if (s_count >= SIM_COPROC_QUEUE_LEN) {
        ESP_LOGW(TAG, "SIM_COPROC: response queue full");
        return false;
    }
    uint8_t wire[PD_PROTO_MAX_FRAME];
    const size_t len = pd_proto_encode(frame, wire, sizeof(wire));
    if (len == 0) {
        return false;
    }
    const size_t idx = (s_head + s_count) % SIM_COPROC_QUEUE_LEN;
    memcpy(s_queue[idx].bytes, wire, len);
    s_queue[idx].len = len;
    s_count++;
    return true;
}

pd_hal_status_t pd_hal_coproc_init(void) {
    s_head = 0;
    s_count = 0;
    s_auto_ack = false;
    s_fault = SIM_COPROC_FAULT_NONE;
    ESP_LOGI(TAG, "SIM_COPROC: init (auto-ack off, no faults)");
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_coproc_send(const uint8_t *buf, size_t len) {
    if (buf == NULL || len == 0 || len > PD_PROTO_MAX_FRAME) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "SIM_COPROC: send len=%d first=%02x", (int)len, buf[0]);

    pd_frame_t frame;
    const int decoded = pd_proto_try_decode(buf, len, &frame);
    if (decoded <= 0) {
        ESP_LOGW(TAG, "SIM_COPROC: sent frame failed to decode (rc=%d)", decoded);
        return PD_HAL_ERR_INVALID_ARG;
    }

    switch (s_fault) {
    case SIM_COPROC_FAULT_JAM: {
        pd_frame_t ev = {.type = PD_FRAME_JAM_EVENT, .seq = frame.seq, .payload_len = 0};
        coproc_enqueue(&ev);
        break;
    }
    case SIM_COPROC_FAULT_EMPTY: {
        pd_frame_t ev = {.type = PD_FRAME_EMPTY_EVENT, .seq = frame.seq, .payload_len = 0};
        coproc_enqueue(&ev);
        break;
    }
    default:
        break;
    }

    if (s_auto_ack && s_fault != SIM_COPROC_FAULT_TIMEOUT) {
        pd_frame_t ack = {.type = PD_FRAME_ACK, .seq = frame.seq, .payload_len = 0};
        if (coproc_enqueue(&ack)) {
            if (s_fault == SIM_COPROC_FAULT_CRC) {
                /* Corrupt the CRC bytes of the queued response. */
                const size_t idx = (s_head + s_count - 1) % SIM_COPROC_QUEUE_LEN;
                s_queue[idx].bytes[s_queue[idx].len - 1] ^= 0xFFu;
            }
            ESP_LOGI(TAG, "SIM_COPROC: queued ACK seq=%02x", frame.seq);
        }
    }
    return PD_HAL_OK;
}

pd_hal_status_t pd_hal_coproc_recv(uint8_t *buf, size_t cap, size_t timeout_ms, size_t *out_len) {
    if (buf == NULL || cap == 0 || out_len == NULL) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    (void)timeout_ms; /* the model responds instantly; no virtual link delay */
    *out_len = 0;
    if (s_count == 0) {
        return PD_HAL_ERR_TIMEOUT;
    }
    const size_t idx = s_head;
    if (s_queue[idx].len > cap) {
        return PD_HAL_ERR_INVALID_ARG;
    }
    memcpy(buf, s_queue[idx].bytes, s_queue[idx].len);
    *out_len = s_queue[idx].len;
    s_head = (s_head + 1) % SIM_COPROC_QUEUE_LEN;
    s_count--;
    return PD_HAL_OK;
}
