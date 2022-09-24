#include "binary.h"


namespace binary
{

    uint16_t BigEndian::Uint16(const char* b)
    {
        return uint16_t(b[1]) | uint16_t(b[0]) << 8;
    }
    uint32_t BigEndian::Uint32(const char* b)
    {
        return uint32_t(b[3]) | uint32_t(b[2]) << 8 | uint32_t(b[1]) << 16 | uint32_t(b[0]) << 24;
    }
    void BigEndian::PutUint16(char* b, uint16_t v)
    {
        b[0] = uint8_t(v >> 8);
        b[1] = uint8_t(v);
    }
    void BigEndian::PutUint32(char* b, uint32_t v)
    {
        b[0] = uint8_t(v >> 24);
        b[1] = uint8_t(v >> 16);
        b[2] = uint8_t(v >> 8);
        b[3] = uint8_t(v);
    }

}
