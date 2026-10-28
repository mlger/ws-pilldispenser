#include "core_protocol.h"

uint16_t pd_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

size_t pd_proto_encode(const pd_frame_t *f, uint8_t *out, size_t out_cap)
{
    if (f == NULL || out == NULL) {
        return 0;
    }
    if (f->payload_len > PD_PROTO_MAX_PAYLOAD) {
        return 0;
    }
    const size_t total = PD_PROTO_HEADER_LEN + (size_t)f->payload_len + PD_PROTO_CRC_LEN;
    if (out_cap < total) {
        return 0;
    }

    out[0] = PD_PROTO_SOF;
    out[1] = PD_PROTO_VER;
    out[2] = f->type;
    out[3] = f->seq;
    out[4] = f->payload_len;
    for (uint8_t i = 0; i < f->payload_len; i++) {
        out[PD_PROTO_HEADER_LEN + i] = f->payload[i];
    }

    const uint16_t crc = pd_crc16(out, PD_PROTO_HEADER_LEN + (size_t)f->payload_len);
    out[PD_PROTO_HEADER_LEN + f->payload_len] = (uint8_t)(crc >> 8);
    out[PD_PROTO_HEADER_LEN + f->payload_len + 1] = (uint8_t)(crc & 0xFFu);
    return total;
}

int pd_proto_try_decode(const uint8_t *buf, size_t len, pd_frame_t *out)
{
    if (buf == NULL || out == NULL) {
        return -1;
    }
    if (len < PD_PROTO_HEADER_LEN) {
        return 0;
    }
    if (buf[0] != PD_PROTO_SOF || buf[1] != PD_PROTO_VER) {
        return -1;
    }

    const size_t plen = buf[4];
    if (plen > PD_PROTO_MAX_PAYLOAD) {
        return -1;
    }
    const size_t total = PD_PROTO_HEADER_LEN + plen + PD_PROTO_CRC_LEN;
    if (len < total) {
        return 0;
    }

    const uint16_t want =
        (uint16_t)(((uint16_t)buf[PD_PROTO_HEADER_LEN + plen] << 8) |
                   (uint16_t)buf[PD_PROTO_HEADER_LEN + plen + 1]);
    if (pd_crc16(buf, PD_PROTO_HEADER_LEN + plen) != want) {
        return -1;
    }

    out->type = buf[2];
    out->seq = buf[3];
    out->payload_len = (uint8_t)plen;
    for (size_t i = 0; i < plen; i++) {
        out->payload[i] = buf[PD_PROTO_HEADER_LEN + i];
    }
    return (int)total;
}
