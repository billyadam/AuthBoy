#ifndef BN_MEMORY_H
#define BN_MEMORY_H

#include <cstdlib>

namespace bn::memory
{
    inline void* ewram_alloc(int bytes)
    {
        return std::malloc(size_t(bytes));
    }

    inline void ewram_free(void* ptr)
    {
        std::free(ptr);
    }
}

#endif
