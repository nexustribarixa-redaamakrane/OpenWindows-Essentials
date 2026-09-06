/*
 * sfontview.c - OpenWindows SuperUnicode Font (.suf) Previewer (.owx)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../DLL/sufrender/sufrender.h"

int sfontview_inspect(const suf_font_face_t *font)
{
    if (!font) return -1;

    /* Render sample glyph */
    const uint8_t *glyph = sufrender_get_glyph(font, 0x0041); /* 'A' */
    (void)glyph;

    return 0;
}
