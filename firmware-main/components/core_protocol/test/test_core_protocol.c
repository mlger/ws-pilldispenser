/*
 * Unity tests for core_protocol. Run on the host (CTest, firmware-main/host_test)
 * and on ESP32-S3 under QEMU (firmware-main/test/apps/unit).
 */
#include <string.h>

#include "core_protocol.h"
#include "unity.h"

TEST_CASE("crc16 ccitt-false check value", "[core_protocol]") {
    TEST_ASSERT_EQUAL_HEX16(0x29B1, pd_crc16((const uint8_t *)"123456789", 9));
}

TEST_CASE("encode decode roundtrip", "[core_protocol]") {
    pd_frame_t f = {0};
    f.type = PD_FRAME_DISPENSE_REQ;
    f.seq = 42;
    f.payload_len = 2;
    f.payload[0] = 3; /* slot */
    f.payload[1] = 1; /* pills */

    uint8_t buf[PD_PROTO_MAX_FRAME];
    const size_t n = pd_proto_encode(&f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(PD_PROTO_HEADER_LEN + 2 + PD_PROTO_CRC_LEN, n);
    TEST_ASSERT_EQUAL_HEX8(PD_PROTO_SOF, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(PD_PROTO_VER, buf[1]);

    pd_frame_t g = {0};
    const int used = pd_proto_try_decode(buf, n, &g);
    TEST_ASSERT_TRUE(used > 0);
    TEST_ASSERT_EQUAL_UINT32(n, (size_t)used);
    TEST_ASSERT_EQUAL_UINT8(PD_FRAME_DISPENSE_REQ, g.type);
    TEST_ASSERT_EQUAL_UINT8(42, g.seq);
    TEST_ASSERT_EQUAL_UINT8(2, g.payload_len);
    TEST_ASSERT_EQUAL_UINT8(3, g.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(1, g.payload[1]);
}

TEST_CASE("empty payload frame roundtrip", "[core_protocol]") {
    pd_frame_t f = {0};
    f.type = PD_FRAME_STATUS_REQ;
    f.seq = 7;
    f.payload_len = 0;

    uint8_t buf[PD_PROTO_MAX_FRAME];
    const size_t n = pd_proto_encode(&f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(PD_PROTO_HEADER_LEN + PD_PROTO_CRC_LEN, n);

    pd_frame_t g = {0};
    const int used = pd_proto_try_decode(buf, n, &g);
    TEST_ASSERT_TRUE(used > 0);
    TEST_ASSERT_EQUAL_UINT8(PD_FRAME_STATUS_REQ, g.type);
    TEST_ASSERT_EQUAL_UINT8(0, g.payload_len);
}

TEST_CASE("crc corruption rejected", "[core_protocol]") {
    pd_frame_t f = {0};
    f.type = PD_FRAME_HEARTBEAT;
    f.seq = 1;
    f.payload_len = 0;

    uint8_t buf[PD_PROTO_MAX_FRAME];
    const size_t n = pd_proto_encode(&f, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 0);
    buf[n - 1] ^= 0xFFu;

    pd_frame_t g = {0};
    TEST_ASSERT_EQUAL_INT(-1, pd_proto_try_decode(buf, n, &g));
}

TEST_CASE("payload corruption rejected", "[core_protocol]") {
    pd_frame_t f = {0};
    f.type = PD_FRAME_DISPENSE_REQ;
    f.seq = 9;
    f.payload_len = 2;
    f.payload[0] = 1;
    f.payload[1] = 2;

    uint8_t buf[PD_PROTO_MAX_FRAME];
    const size_t n = pd_proto_encode(&f, buf, sizeof(buf));
    buf[5] ^= 0x01u; /* flip a payload bit */

    pd_frame_t g = {0};
    TEST_ASSERT_EQUAL_INT(-1, pd_proto_try_decode(buf, n, &g));
}

TEST_CASE("truncated frame needs more bytes", "[core_protocol]") {
    pd_frame_t f = {0};
    f.type = PD_FRAME_DISPENSE_REQ;
    f.seq = 1;
    f.payload_len = 4;
    memset(f.payload, 0xAA, 4);

    uint8_t buf[PD_PROTO_MAX_FRAME];
    const size_t n = pd_proto_encode(&f, buf, sizeof(buf));

    pd_frame_t g = {0};
    TEST_ASSERT_EQUAL_INT(0, pd_proto_try_decode(buf, n - 1, &g));
    TEST_ASSERT_EQUAL_INT(0, pd_proto_try_decode(buf, PD_PROTO_HEADER_LEN, &g));
}

TEST_CASE("bad sof rejected", "[core_protocol]") {
    uint8_t buf[8] = {0x5A, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00};
    pd_frame_t g = {0};
    TEST_ASSERT_EQUAL_INT(-1, pd_proto_try_decode(buf, sizeof(buf), &g));
}

TEST_CASE("oversized payload rejected on encode", "[core_protocol]") {
    pd_frame_t f = {0};
    f.payload_len = PD_PROTO_MAX_PAYLOAD + 1;
    uint8_t buf[PD_PROTO_MAX_FRAME];
    TEST_ASSERT_EQUAL_UINT32(0, pd_proto_encode(&f, buf, sizeof(buf)));
}

TEST_CASE("insufficient output buffer rejected", "[core_protocol]") {
    pd_frame_t f = {0};
    f.type = PD_FRAME_HEARTBEAT;
    f.seq = 1;
    f.payload_len = 0;
    uint8_t small[4];
    TEST_ASSERT_EQUAL_UINT32(0, pd_proto_encode(&f, small, sizeof(small)));
}
