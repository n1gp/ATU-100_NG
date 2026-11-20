// sim/sim_main.c
#include <stdint.h>
#include "display.h"
#include "oled.h"

void sim_render(void);

#define FW_VERSION "1.2.1"
#define CENTRE_BIG(s)   ((128 - 12 * (sizeof(s) - 1) + 2) / 2)
#define CENTRE_SMALL(s) ((128 - 6 * (sizeof(s) - 1) + 1) / 2)
#define GREET_FW "NG " FW_VERSION

int main(void) {
    oled_init();
    disp_cells(0, 4, 0b0101101);
    disp_cells(8, 4, 0b1110010);
    disp_big(20, 4, "14.074");
    sim_render();
   oled_clear();
   disp_big(CLINE1, 0, 0 ? "BYP" : "SWR");
   disp_big(CLINE1, 42, "=");
   disp_med(CLINE1, 60, "-.--");
   disp_big(CLINE2, 98, "W");
   disp_big(CLINE2, 0, "PWR");
   disp_big(CLINE2, 42, "=");
   disp_big(CLINE2, 98, "W");
   disp_big(CLINE3, 0, "L=");
   disp_big(CLINE3, 98, "uH");
   disp_big(CLINE4, 0, "C=");
   disp_big(CLINE4, 98, "pF");
   disp_big(CLINE3, 42, "80.5");
   disp_big(CLINE4, 42, "1712");
    sim_render();

    return 0;
}

