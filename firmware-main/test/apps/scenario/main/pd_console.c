#include "pd_console.h"

#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"

#include "core_fsm.h"
#include "core_scheduler.h"
#include "fsm_svc.h"
#include "hal_rtc.h"
#include "scheduler_svc.h"
#include "storage.h"

static const char *TAG = "pd_cmd";

static void print_status(void) {
    time_t now = 0;
    (void)pd_hal_rtc_get(&now);
    pd_sched_slot_t slots[PD_SCHED_MAX_SLOTS];
    const size_t n = pd_sched_svc_get_slots(slots, PD_SCHED_MAX_SLOTS);
    ESP_LOGI(TAG, "PD: state=%s slot=%u key=%llu now=%lld", pd_fsm_svc_state_name(),
             (unsigned)pd_fsm_svc_slot(), (unsigned long long)pd_fsm_svc_key(), (long long)now);
    pd_journal_entry_t e;
    if (pd_storage_journal_pending(&e)) {
        ESP_LOGI(TAG, "PD: journal=pending slot=%u key=%llu", (unsigned)e.slot_id,
                 (unsigned long long)e.dose_key);
    } else {
        ESP_LOGI(TAG, "PD: journal=none");
    }
    for (size_t i = 0; i < n; i++) {
        char iso[24] = "-";
        if (slots[i].enabled) {
            const time_t next = pd_sched_next_due(&slots[i], now);
            if (next > 0) {
                pd_sched_format_iso(next, iso, sizeof(iso));
            }
        }
        ESP_LOGI(TAG, "PD: slot=%u %02u:%02u rec=%u en=%d next=%s", (unsigned)slots[i].id,
                 (unsigned)slots[i].hour, (unsigned)slots[i].minute, (unsigned)slots[i].recurrence,
                 (int)slots[i].enabled, iso);
    }
}

static int parse_hhmm(const char *text, uint8_t *hour, uint8_t *minute) {
    unsigned h = 0, m = 0;
    if (sscanf(text, "%u:%u", &h, &m) != 2 || h > 23 || m > 59) {
        return -1;
    }
    *hour = (uint8_t)h;
    *minute = (uint8_t)m;
    return 0;
}

static int parse_mask(const char *text, uint8_t *mask) {
    /* "MTWTFSS", '.' or '-' = off, any other char = on. bit0=Mon..bit6=Sun. */
    if (strlen(text) != 7) {
        return -1;
    }
    uint8_t m = 0;
    for (int i = 0; i < 7; i++) {
        if (text[i] != '.' && text[i] != '-') {
            m |= (uint8_t)(1u << i);
        }
    }
    *mask = m;
    return 0;
}

/* argv here is the full `pd slots ...` command: argv[1] = "slots",
 * argv[2] = "list"|"set", argv[3] = <id>, argv[4] = <HH:MM>, argv[5] = rec,
 * argv[6] = optional weekly mask. */
static int cmd_slots(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[2], "list") == 0) {
        print_status();
        return 0;
    }
    if (argc >= 6 && strcmp(argv[2], "set") == 0) {
        unsigned id = 0;
        if (sscanf(argv[3], "%u", &id) != 1 || id == 0 || id > PD_SCHED_MAX_SLOTS) {
            ESP_LOGW(TAG, "invalid slot id '%s'", argv[3]);
            return 1;
        }
        uint8_t hour = 0, minute = 0;
        if (parse_hhmm(argv[4], &hour, &minute) != 0) {
            ESP_LOGW(TAG, "invalid time '%s' (use HH:MM)", argv[4]);
            return 1;
        }
        pd_sched_recurrence_t rec = PD_SCHED_DAILY;
        if (strcmp(argv[5], "once") == 0) {
            rec = PD_SCHED_ONCE;
        } else if (strcmp(argv[5], "daily") == 0) {
            rec = PD_SCHED_DAILY;
        } else if (strcmp(argv[5], "weekly") == 0) {
            rec = PD_SCHED_WEEKLY;
        } else {
            ESP_LOGW(TAG, "invalid recurrence '%s' (once|daily|weekly)", argv[5]);
            return 1;
        }
        uint8_t mask = 0x7f;
        if (rec == PD_SCHED_WEEKLY) {
            if (argc < 7 || parse_mask(argv[6], &mask) != 0) {
                ESP_LOGW(TAG, "weekly needs a 7-char mask (e.g. MTWTFSS or XXXXX..)");
                return 1;
            }
        }

        pd_sched_slot_t slots[PD_SCHED_MAX_SLOTS];
        size_t n = pd_sched_svc_get_slots(slots, PD_SCHED_MAX_SLOTS);
        size_t idx = n;
        for (size_t i = 0; i < n; i++) {
            if (slots[i].id == (uint8_t)id) {
                idx = i;
            }
        }
        if (idx == n) {
            if (n >= PD_SCHED_MAX_SLOTS) {
                ESP_LOGW(TAG, "slot table full");
                return 1;
            }
            n++;
        }
        memset(&slots[idx], 0, sizeof(slots[idx]));
        slots[idx].id = (uint8_t)id;
        slots[idx].recurrence = rec;
        slots[idx].hour = hour;
        slots[idx].minute = minute;
        slots[idx].days_mask = mask;
        slots[idx].enabled = true;

        if (pd_sched_svc_set_slots(slots, n) != 0) {
            ESP_LOGW(TAG, "slot persist failed");
            return 1;
        }
        ESP_LOGI(TAG, "PD: slot=%u set %02u:%02u rec=%u", (unsigned)id, (unsigned)hour,
                 (unsigned)minute, (unsigned)rec);
        return 0;
    }
    ESP_LOGW(TAG, "usage: pd slots list | pd slots set <id> <HH:MM> <once|daily|weekly> [mask]");
    return 1;
}

/* argv here is the full `pd dose ...` command: argv[2] = "trigger"|"reboot",
 * argv[3] = <slot>. */
static int cmd_dose(int argc, char **argv) {
    if (argc != 4) {
        ESP_LOGW(TAG, "usage: pd dose trigger|reboot <slot>");
        return 1;
    }
    unsigned id = 0;
    if (sscanf(argv[3], "%u", &id) != 1 || id == 0) {
        ESP_LOGW(TAG, "invalid slot id '%s'", argv[3]);
        return 1;
    }
    time_t now = 0;
    if (pd_hal_rtc_get(&now) != PD_HAL_OK) {
        ESP_LOGW(TAG, "rtc unavailable");
        return 1;
    }
    const uint64_t key = pd_sched_dose_key(now);
    if (strcmp(argv[2], "reboot") == 0) {
        pd_fsm_svc_arm_power_loss();
        ESP_LOGI(TAG, "PD: dose reboot slot=%u key=%llu (power-loss emulation)", id,
                 (unsigned long long)key);
    } else {
        ESP_LOGI(TAG, "PD: dose trigger slot=%u key=%llu", id, (unsigned long long)key);
    }
    (void)pd_fsm_svc_dose_due((uint8_t)id, key);
    return 0;
}

static int cmd_pd_dispatch(int argc, char **argv) {
    if (argc < 2) {
        print_status();
        return 0;
    }
    if (strcmp(argv[1], "status") == 0 && argc == 2) {
        print_status();
        return 0;
    }
    if (strcmp(argv[1], "slots") == 0) {
        if (argc >= 3) {
            return cmd_slots(argc, argv);
        }
        ESP_LOGW(TAG, "usage: pd slots list | pd slots set <id> <HH:MM> <once|daily|weekly>");
        return 1;
    }
    if (strcmp(argv[1], "dose") == 0) {
        return cmd_dose(argc, argv);
    }
    if (strcmp(argv[1], "reset") == 0 && argc == 2) {
        ESP_LOGI(TAG, "PD: operator reset");
        return pd_fsm_svc_reset() == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "retry") == 0 && argc == 2) {
        ESP_LOGI(TAG, "PD: operator retry");
        return pd_fsm_svc_retry() == 0 ? 0 : 1;
    }
    ESP_LOGW(TAG, "unknown pd subcommand '%s'", argv[1]);
    return 1;
}

esp_err_t pd_console_register(void) {
    /* esp_console dispatches on the exact command name; subcommands are
     * handled inside one handler, mirroring the `sim` command design. */
    const esp_console_cmd_t cmd = {
        .command = "pd",
        .help = "PD ops: pd status | pd slots list | "
                "pd slots set <id> <HH:MM> <once|daily|weekly> [MTWTFSS] | "
                "pd dose trigger <slot> | pd dose reboot <slot> | pd reset | pd retry",
        .hint = NULL,
        .func = &cmd_pd_dispatch,
        .argtable = NULL};
    const esp_err_t err = esp_console_cmd_register(&cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pd console register failed: %s", esp_err_to_name(err));
    }
    return err;
}
