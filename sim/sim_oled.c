// sim/sim_oled.c
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "oled.h"

static uint8_t gddram[8][128] = {0};

uint8_t oled_init(void) {
    memset(gddram, 0, sizeof(gddram));
    return 0;
}

void oled_clear(void) {
    memset(gddram, 0, sizeof(gddram));
}

uint8_t oled_write(uint8_t page, uint8_t col, const uint8_t *buf, uint8_t len) {
    if (page >= 8 || col >= 128) return 0;
    if (col + len > 128) len = 128 - col;
    memcpy(&gddram[page][col], buf, len);
    return 0;
}

// Add stubs here if oled.h declares any other functions (e.g. oled_contrast, oled_invert)

void sim_render(void) {
    printf("+");
    for (int x = 0; x < 128; x++) printf("-");
    printf("+\n");

    for (int y = 0; y < 64; y += 2) {
        printf("|");
        for (int x = 0; x < 128; x++) {
            uint8_t top = (gddram[y / 8][x] >> (y % 8)) & 1;
            uint8_t bot = (gddram[(y + 1) / 8][x] >> ((y + 1) % 8)) & 1;
            if (top && bot) printf("\xE2\x96\x88"); // █
            else if (top)   printf("\xE2\x96\x80"); // ▀
            else if (bot)   printf("\xE2\x96\x84"); // ▄
            else            printf(" ");
        }
        printf("|\n");
    }

    printf("+");
    for (int x = 0; x < 128; x++) printf("-");
    printf("+\n");
}

