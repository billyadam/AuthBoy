#include "entropy.h"

#include "bn_core.h"
#include "bn_date.h"
#include "bn_time.h"
#include "bn_timer.h"
#include "bn_keypad.h"
#include "bn_optional.h"

extern "C"
{
    #include "monocypher.h"
}

namespace app
{

namespace
{
    crypto_blake2b_ctx pool;
    bn::optional<bn::timer> timer;
    uint32_t frame_counter = 0;
    uint32_t output_counter = 0;
    uint16_t last_keys = 0;

    constexpr bn::keypad::key_type keys[] = {
        bn::keypad::key_type::A, bn::keypad::key_type::B, bn::keypad::key_type::SELECT,
        bn::keypad::key_type::START, bn::keypad::key_type::RIGHT, bn::keypad::key_type::LEFT,
        bn::keypad::key_type::UP, bn::keypad::key_type::DOWN, bn::keypad::key_type::R,
        bn::keypad::key_type::L,
    };

    uint16_t read_keys()
    {
        uint16_t bits = 0;

        for(int i = 0; i < int(sizeof(keys) / sizeof(keys[0])); ++i)
        {
            if(bn::keypad::held(keys[i]))
            {
                bits |= uint16_t(1 << i);
            }
        }

        return bits;
    }
}

void entropy_init()
{
    crypto_blake2b_init(&pool, 64);
    timer.emplace();

    struct
    {
        int year, month, day, hour, minute, second;
    } rtc = {};

    if(bn::optional<bn::date> date = bn::date::current())
    {
        rtc.year = date->year();
        rtc.month = date->month();
        rtc.day = date->month_day();
    }

    if(bn::optional<bn::time> time = bn::time::current())
    {
        rtc.hour = time->hour();
        rtc.minute = time->minute();
        rtc.second = time->second();
    }

    entropy_add(&rtc, sizeof(rtc));
}

void entropy_add(const void* data, int size)
{
    crypto_blake2b_update(&pool, static_cast<const uint8_t*>(data), size_t(size));
}

void frame_update()
{
    ++frame_counter;

    uint16_t keys_now = read_keys();

    // Mix in the frame number and timer ticks whenever any button changes state.
    if(keys_now != last_keys && timer)
    {
        struct
        {
            uint32_t frame;
            int32_t ticks;
            uint16_t keys;
        } sample = { frame_counter, timer->elapsed_ticks(), keys_now };

        entropy_add(&sample, sizeof(sample));
        last_keys = keys_now;
    }

    bn::core::update();
}

void random_bytes(uint8_t* output, int size)
{
    while(size > 0)
    {
        // Hash a copy of the pool plus a counter, so the pool itself keeps accumulating.
        crypto_blake2b_ctx copy = pool;
        struct
        {
            uint32_t counter;
            uint32_t frame;
            int32_t ticks;
        } extra = { ++output_counter, frame_counter, timer ? timer->elapsed_ticks() : 0 };

        crypto_blake2b_update(&copy, reinterpret_cast<const uint8_t*>(&extra), sizeof(extra));

        uint8_t block[64];
        crypto_blake2b_final(&copy, block);

        int chunk = size < 64 ? size : 64;

        for(int i = 0; i < chunk; ++i)
        {
            output[i] = block[i];
        }

        crypto_wipe(block, sizeof(block));
        output += chunk;
        size -= chunk;
    }
}

}
