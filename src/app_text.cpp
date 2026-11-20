#include "app_text.h"

#include "common_fixed_8x16_sprite_font.h"
#include "common_variable_8x16_sprite_font.h"

namespace app
{

text_generators::text_generators() :
    small(common::variable_8x16_sprite_font),
    mono(common::fixed_8x16_sprite_font)
{
    small.set_center_alignment();
    mono.set_center_alignment();
}

}
