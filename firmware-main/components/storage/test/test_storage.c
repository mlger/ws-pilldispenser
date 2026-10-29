/*
 * Unity tests for the NVS-backed storage component. Run on ESP32-S3 under
 * QEMU only (NVS needs a real flash partition; the host CTest build does not
 * compile this component). The unit test app initializes NVS before running
 * the menu.
 */
#include <string.h>

#include "core_scheduler.h"
#include "storage.h"
#include "unity.h"

/* Every case starts from the same clean namespace: IDF unity provides weak
 * global setUp/tearDown (unity_compat.c) and the host build has its own weak
 * defaults, so per-suite setup lives in this helper instead. NVS persists
 * across cases within one app run, hence the erase. */
static void storage_ready(void) {
    TEST_ASSERT_EQUAL_INT(0, pd_storage_init());
    TEST_ASSERT_EQUAL_INT(0, pd_storage_erase_all());
}

static pd_sched_slot_t make_slot(uint8_t id, uint8_t hour, uint8_t min) {
    pd_sched_slot_t s = {0};
    s.id = id;
    s.enabled = true;
    s.recurrence = PD_SCHED_DAILY;
    s.hour = hour;
    s.minute = min;
    return s;
}

TEST_CASE("journal begin pending commit clears", "[storage]") {
    storage_ready();
    pd_journal_entry_t e = {0};
    TEST_ASSERT_FALSE(pd_storage_journal_pending(&e));

    TEST_ASSERT_EQUAL_INT(0, pd_storage_journal_begin(2, 202603290300ull));
    TEST_ASSERT_TRUE(pd_storage_journal_pending(&e));
    TEST_ASSERT_EQUAL_UINT8(2, e.slot_id);
    TEST_ASSERT_EQUAL_UINT64(202603290300ull, e.dose_key);

    TEST_ASSERT_EQUAL_INT(0, pd_storage_journal_commit());
    TEST_ASSERT_FALSE(pd_storage_journal_pending(&e));
}

TEST_CASE("journal begin overwrites previous pending", "[storage]") {
    storage_ready();
    pd_journal_entry_t e = {0};
    TEST_ASSERT_EQUAL_INT(0, pd_storage_journal_begin(1, 111ull));
    TEST_ASSERT_EQUAL_INT(0, pd_storage_journal_begin(3, 222ull));
    TEST_ASSERT_TRUE(pd_storage_journal_pending(&e));
    TEST_ASSERT_EQUAL_UINT8(3, e.slot_id);
    TEST_ASSERT_EQUAL_UINT64(222ull, e.dose_key);
}

TEST_CASE("journal resolve path also clears pending", "[storage]") {
    storage_ready();
    TEST_ASSERT_EQUAL_INT(0, pd_storage_journal_begin(1, 333ull));
    /* Boot replay resolves via the same commit primitive. */
    TEST_ASSERT_EQUAL_INT(0, pd_storage_journal_commit());
    pd_journal_entry_t e = {0};
    TEST_ASSERT_FALSE(pd_storage_journal_pending(&e));
}

TEST_CASE("slots save and load roundtrip", "[storage]") {
    storage_ready();
    pd_sched_slot_t slots[3];
    slots[0] = make_slot(1, 8, 0);
    slots[1] = make_slot(2, 20, 30);
    slots[2] = make_slot(3, 12, 15);
    slots[2].recurrence = PD_SCHED_WEEKLY;
    slots[2].days_mask = 0x1c;
    slots[2].enabled = false;

    TEST_ASSERT_EQUAL_INT(0, pd_storage_slots_save(slots, 3));

    pd_sched_slot_t out[PD_STORAGE_MAX_SLOTS] = {0};
    size_t n = PD_STORAGE_MAX_SLOTS;
    TEST_ASSERT_EQUAL_INT(0, pd_storage_slots_load(out, &n));
    TEST_ASSERT_EQUAL_UINT32(3, (unsigned)n);
    TEST_ASSERT_EQUAL_MEMORY(slots, out, 3 * sizeof(pd_sched_slot_t));
}

TEST_CASE("slots load with nothing stored", "[storage]") {
    storage_ready();
    pd_sched_slot_t out[4] = {0};
    size_t n = 4;
    TEST_ASSERT_EQUAL_INT(0, pd_storage_slots_load(out, &n));
    TEST_ASSERT_EQUAL_UINT32(0, (unsigned)n);
}

TEST_CASE("last key set and get per slot", "[storage]") {
    storage_ready();
    uint64_t key = 0;
    TEST_ASSERT_EQUAL_INT(0, pd_storage_last_key_get(1, &key));
    TEST_ASSERT_EQUAL_UINT64(0, key); /* never fired */

    TEST_ASSERT_EQUAL_INT(0, pd_storage_last_key_set(1, 202601010800ull));
    TEST_ASSERT_EQUAL_INT(0, pd_storage_last_key_set(2, 202601012000ull));
    TEST_ASSERT_EQUAL_INT(0, pd_storage_last_key_get(1, &key));
    TEST_ASSERT_EQUAL_UINT64(202601010800ull, key);
    TEST_ASSERT_EQUAL_INT(0, pd_storage_last_key_get(2, &key));
    TEST_ASSERT_EQUAL_UINT64(202601012000ull, key);
    TEST_ASSERT_EQUAL_INT(0, pd_storage_last_key_set(1, 202601020800ull));
    TEST_ASSERT_EQUAL_INT(0, pd_storage_last_key_get(1, &key));
    TEST_ASSERT_EQUAL_UINT64(202601020800ull, key);
}

TEST_CASE("storage rejects invalid arguments", "[storage]") {
    storage_ready();
    pd_sched_slot_t slots[1] = {make_slot(1, 8, 0)};
    TEST_ASSERT_EQUAL_INT(-1, pd_storage_slots_save(NULL, 1));
    TEST_ASSERT_EQUAL_INT(-1, pd_storage_slots_save(slots, 0));
    TEST_ASSERT_EQUAL_INT(-1, pd_storage_last_key_set(0, 1ull));
    TEST_ASSERT_EQUAL_INT(-1, pd_storage_last_key_get(0, &(uint64_t){0}));
    TEST_ASSERT_EQUAL_INT(0, pd_storage_journal_begin(1, 1ull)); /* valid, sanity */
    TEST_ASSERT_EQUAL_INT(0, pd_storage_journal_commit());
}
