#include "kernel.h"
#include "drivers/screen.h"
#include "gfx/gfx.h"

#define NUM_STARS 120
#define CX (SCREEN_WIDTH  / 2)
#define CY (SCREEN_HEIGHT / 2)
#define DEPTH_MAX 256

typedef struct {
    int x, y, z;
    int px, py;   /* previous screen position for erase */
} Star;

static Star _stars[NUM_STARS];
static unsigned int _rng = 0xDEADBEEF;

static unsigned int _rand(void) {
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return _rng;
}

static void _reset_star(Star* s) {
    s->x  = (int)(_rand() % SCREEN_WIDTH)  - CX;
    s->y  = (int)(_rand() % SCREEN_HEIGHT) - CY;
    s->z  = (int)(_rand() % DEPTH_MAX) + 1;
    s->px = -1; s->py = -1;
}

void screensaver_init(void) {
    for (int i = 0; i < NUM_STARS; i++) _reset_star(&_stars[i]);
}

void screensaver_tick(void) {
    for (int i = 0; i < NUM_STARS; i++) {
        _stars[i].z -= 4;
        if (_stars[i].z <= 0) _reset_star(&_stars[i]);
    }
}

void screensaver_render(void) {
    clear_screen(COLOR_BLACK);
    for (int i = 0; i < NUM_STARS; i++) {
        Star* s = &_stars[i];
        if (s->z <= 0) continue;
        int sx = CX + s->x * DEPTH_MAX / s->z;
        int sy = CY + s->y * DEPTH_MAX / s->z;
        if ((unsigned)sx >= SCREEN_WIDTH || (unsigned)sy >= SCREEN_HEIGHT) {
            _reset_star(s); continue;
        }
        /* brightness: closer = brighter */
        int bright = 255 - (s->z * 255 / DEPTH_MAX);
        if (bright < 40) bright = 40;
        uint8_t b = (uint8_t)bright;
        uint32_t col = (((uint32_t)b << 16) | ((uint32_t)b << 8) | b);
        /* size based on depth */
        int size = (DEPTH_MAX - s->z) / 64 + 1;
        if (size > 3) size = 3;
        if (size == 1) put_pixel(sx, sy, col);
        else           gfx_fill_circle(sx, sy, size, col);
        s->px = sx; s->py = sy;
    }

    /* VOS logo center fade-in */
    draw_string(CX-28, CY-8, "VanillaOS", RGB(60,80,120), COLOR_TRANSPARENT);
    screen_flip();
}
