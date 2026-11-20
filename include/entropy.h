#ifndef ENTROPY_H
#define ENTROPY_H

#include <cstdint>

namespace app
{
    // The GBA has no hardware random number generator. Randomness is gathered into a
    // BLAKE2b pool from the RTC, a high-resolution timer and the exact timing of every
    // button press and release. A counter makes each output unique even if the pool
    // gathered little; salts and nonces only need to be unique and unpredictable.

    void entropy_init();

    // Mixes extra data into the pool (e.g. the previous save's contents).
    void entropy_add(const void* data, int size);

    // Use instead of bn::core::update(): gathers entropy from input timing each frame.
    void frame_update();

    void random_bytes(uint8_t* output, int size);
}

#endif
