/*
 * Unity tests for core_inventory. Run on the host (CTest) and on ESP32-S3
 * under QEMU.
 */
#include <string.h>

#include "unity.h"
#include "core_inventory.h"

void setUp(void)
{
}

void tearDown(void)
{
}

TEST_CASE("init and take", "[core_inventory]")
{
    pd_inventory_t inv;
    TEST_ASSERT_TRUE(pd_inv_init(&inv, 4));

    TEST_ASSERT_TRUE(pd_inv_load_slot(&inv, 1, 30, 10, "aspirin"));
    TEST_ASSERT_EQUAL_UINT16(10, pd_inv_remaining(&inv, 1));

    TEST_ASSERT_TRUE(pd_inv_take(&inv, 1, 1));
    TEST_ASSERT_EQUAL_UINT16(9, pd_inv_remaining(&inv, 1));
}

TEST_CASE("insufficient pills refused atomically", "[core_inventory]")
{
    pd_inventory_t inv;
    TEST_ASSERT_TRUE(pd_inv_init(&inv, 4));
    TEST_ASSERT_TRUE(pd_inv_load_slot(&inv, 2, 30, 2, "ibuprofen"));

    TEST_ASSERT_FALSE(pd_inv_take(&inv, 2, 3));
    TEST_ASSERT_EQUAL_UINT16(2, pd_inv_remaining(&inv, 2)); /* no partial take */
}

TEST_CASE("unknown slot refused", "[core_inventory]")
{
    pd_inventory_t inv;
    TEST_ASSERT_TRUE(pd_inv_init(&inv, 2));
    TEST_ASSERT_FALSE(pd_inv_take(&inv, 9, 1));
    TEST_ASSERT_EQUAL_UINT16(0, pd_inv_remaining(&inv, 9));
}

TEST_CASE("low slot detection", "[core_inventory]")
{
    pd_inventory_t inv;
    TEST_ASSERT_TRUE(pd_inv_init(&inv, 8));
    TEST_ASSERT_TRUE(pd_inv_load_slot(&inv, 1, 30, 5, "a"));
    TEST_ASSERT_TRUE(pd_inv_load_slot(&inv, 2, 30, 25, "b"));
    TEST_ASSERT_TRUE(pd_inv_load_slot(&inv, 3, 30, 0, "c"));

    uint8_t low[8];
    const uint8_t n = pd_inv_low_slots(&inv, 5, low, 8);
    TEST_ASSERT_EQUAL_UINT8(2, n);
    TEST_ASSERT_EQUAL_UINT8(1, low[0]);
    TEST_ASSERT_EQUAL_UINT8(3, low[1]);
}

TEST_CASE("count above capacity refused", "[core_inventory]")
{
    pd_inventory_t inv;
    TEST_ASSERT_TRUE(pd_inv_init(&inv, 2));
    TEST_ASSERT_FALSE(pd_inv_load_slot(&inv, 1, 10, 11, "x"));
}

TEST_CASE("slot count bounds", "[core_inventory]")
{
    pd_inventory_t inv;
    TEST_ASSERT_FALSE(pd_inv_init(&inv, 0));
    TEST_ASSERT_FALSE(pd_inv_init(&inv, PD_INV_MAX_SLOTS + 1));
    TEST_ASSERT_TRUE(pd_inv_init(&inv, PD_INV_MAX_SLOTS));
}

TEST_CASE("pill name truncation is safe", "[core_inventory]")
{
    pd_inventory_t inv;
    TEST_ASSERT_TRUE(pd_inv_init(&inv, 1));
    TEST_ASSERT_TRUE(pd_inv_load_slot(&inv, 1, 10, 1,
                                      "012345678901234567890123456789"));
    /* strncpy(dst, src, PD_INV_NAME_MAX - 1) + explicit NUL: exactly 23 chars
     * kept, last byte always terminated. */
    TEST_ASSERT_EQUAL_UINT8('\0', inv.slots[0].pill_name[PD_INV_NAME_MAX - 1]);
    TEST_ASSERT_EQUAL_UINT8(23, strlen(inv.slots[0].pill_name));
}
