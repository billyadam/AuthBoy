// Host stand-in for Butano's SRAM API: a 32 KB byte array the tests can inspect and damage.
#ifndef BN_SRAM_H
#define BN_SRAM_H

#include <cstring>

namespace bn::sram
{
    inline unsigned char data[32 * 1024];

    constexpr int size()
    {
        return int(sizeof(data));
    }

    template<typename Type>
    void read_offset(Type& destination, int offset)
    {
        std::memcpy(&destination, data + offset, sizeof(Type));
    }

    template<typename Type>
    void read(Type& destination)
    {
        read_offset(destination, 0);
    }

    template<typename Type>
    void write_offset(const Type& source, int offset)
    {
        static_assert(sizeof(Type) <= sizeof(data));
        std::memcpy(data + offset, &source, sizeof(Type));
    }

    template<typename Type>
    void write(const Type& source)
    {
        write_offset(source, 0);
    }
}

#endif
