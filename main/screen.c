#include "boards.h"

#if USE_SCREEN

#include <string.h>
#include "lcd.h"

// TODO fix later
#define UF2_VERSION_BASE "0.0.0"

// Overlap 4x chars by this much.
#define CHAR4_KERNING 2
// Width of a single 4x char, adjusted by kerning
#define CHAR4_KERNED_WIDTH  (6 * 4 - CHAR4_KERNING)

spi_device_handle_t _spi;

#define COL0(r, g, b) ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))
#define COL(c) COL0((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff)

const uint16_t palette[] = {
    COL(0x000000), // 0
    COL(0xffffff), // 1
    COL(0xff2121), // 2
    COL(0xff93c4), // 3
    COL(0xff8135), // 4
    COL(0xfff609), // 5
    COL(0x249ca3), // 6
    COL(0x78dc52), // 7
    COL(0x003fad), // 8
    COL(0x87f2ff), // 9
    COL(0x8e2ec4), // 10
    COL(0xa4839f), // 11
    COL(0x5c406c), // 12
    COL(0xe5cdc4), // 13
    COL(0x91463d), // 14
    COL(0x000000), // 15
};

uint8_t fb[DISPLAY_WIDTH * DISPLAY_HEIGHT];
extern const uint8_t font8[];
extern const uint8_t fileLogo[];
extern const uint8_t pendriveLogo[];
extern const uint8_t arrowLogo[];

static void printch(int x, int y, int col, const uint8_t *fnt) {
    for (int i = 0; i < 6; ++i) {
        uint8_t *p = fb + (x + i) * DISPLAY_HEIGHT + y;
        uint8_t mask = 0x01;
        for (int j = 0; j < 8; ++j) {
            if (*fnt & mask)
                *p = col;
            p++;
            mask <<= 1;
        }
        fnt++;
    }
}

static void printch4(int x, int y, int col, const uint8_t *fnt) {
    for (int i = 0; i < 6 * 4; ++i) {
        uint8_t *p = fb + (x + i) * DISPLAY_HEIGHT + y;
        uint8_t mask = 0x01;
        for (int j = 0; j < 8; ++j) {
            for (int k = 0; k < 4; ++k) {
                if (*fnt & mask)
                    *p = col;
                p++;
            }
            mask <<= 1;
        }
        if ((i & 3) == 3)
            fnt++;
    }
}

void printicon(int x, int y, int col, const uint8_t *icon) {
    int w = *icon++;
    int h = *icon++;
    int sz = *icon++;

    uint8_t mask = 0x80;
    int runlen = 0;
    int runbit = 0;
    uint8_t lastb = 0x00;

    for (int i = 0; i < w; ++i) {
        uint8_t *p = fb + (x + i) * DISPLAY_HEIGHT + y;
        for (int j = 0; j < h; ++j) {
            int c = 0;
            if (mask != 0x80) {
                if (lastb & mask)
                    c = 1;
                mask <<= 1;
            } else if (runlen) {
                if (runbit)
                    c = 1;
                runlen--;
            } else {
                if (sz-- <= 0) ESP_LOGE("screen", "Panic code = 10");
                lastb = *icon++;
                if (lastb & 0x80) {
                    runlen = lastb & 63;
                    runbit = lastb & 0x40;
                } else {
                    mask = 0x01;
                }
                --j;
                continue; // restart
            }
            if (c)
                *p = col;
            p++;
        }
    }
}

void print(int x, int y, int col, const char *text) {
    int x0 = x;
    while (*text) {
        char c = *text++;
        if (c == '\r')
            continue;
        if (c == '\n') {
            x = x0;
            y += 10;
            continue;
        }
        /*
        if (x + 8 > DISPLAY_WIDTH) {
            x = x0;
            y += 10;
        }
        */
        if (c < ' ')
            c = '?';
        if (c >= 0x7f)
            c = '?';
        c -= ' ';
        printch(x, y, col, &font8[c * 6]);
        x += 6;
    }
}

void print4(int x, int y, int col, const char *text) {
    while (*text) {
        char c = *text++;
        c -= ' ';
        printch4(x, y, col, &font8[c * 6]);
        x += CHAR4_KERNED_WIDTH;
        if (x + CHAR4_KERNED_WIDTH > DISPLAY_WIDTH) {
            // Next char won't fit.
            return;
        }
    }
}

static void draw_screen(void) {
    uint8_t *p = fb;
    for (int i = 0; i < DISPLAY_WIDTH; ++i) {
        uint8_t cc[DISPLAY_HEIGHT * 2];
        uint32_t dst = 0;
        for (int j = 0; j < DISPLAY_HEIGHT; ++j) {
            uint16_t color = palette[*p++ & 0xf];
            cc[dst++] = color >> 8;
            cc[dst++] = color & 0xff;
        }

        lcd_draw_lines(_spi, i, (uint16_t*) cc);
    }
}

void drawBar(int y, int h, int c) {
    for (int x = 0; x < DISPLAY_WIDTH; ++x) {
        memset(fb + x * DISPLAY_HEIGHT + y, c, h);
    }
}

void screen_draw_hf2(void) {
    print4(20, 22, 5, "<-->");
    print(40, 110, 7, "flashing...");
    draw_screen();
}

void screen_draw_drag(void) {
    drawBar(0, 52, 7);
    drawBar(52, 55, 8);
    drawBar(107, 14, 4);

    // Center UF2_PRODUCT_NAME and UF2_VERSION_BASE.
    int name_x = (DISPLAY_WIDTH - (6 * 4 - CHAR4_KERNING) * (int) strlen(USB_PRODUCT)) / 2;
    print4(name_x >= 0 ? name_x : 0, 5, 1, USB_PRODUCT);
    int version_x = (DISPLAY_WIDTH - 6 * (int) strlen(UF2_VERSION_BASE)) / 2;
    print(version_x >= 0 ? version_x : 0, 40, 6, UF2_VERSION_BASE);
    print(23, 110, 1, "arcade.makecode.com");

#define DRAG 70
#define DRAGX 10
    printicon(DRAGX + 20, DRAG + 5, 1, fileLogo);
    printicon(DRAGX + 66, DRAG, 1, arrowLogo);
    printicon(DRAGX + 108, DRAG, 1, pendriveLogo);
    print(10, DRAG - 12, 1, "arcade.uf2");
    print(90, DRAG - 12, 1, UF2_VOLUME_LABEL);

    draw_screen();
}


void screen_init(void)
{
  spi_bus_config_t bus_cfg = {
    .miso_io_num     = PIN_DISPLAY_MISO,
    .mosi_io_num     = PIN_DISPLAY_MOSI,
    .sclk_io_num     = PIN_DISPLAY_SCK,
    .quadwp_io_num   = -1,
    .quadhd_io_num   = -1,
    .max_transfer_sz = PARALLEL_LINES * 320 * 2 + 8
  };

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000,              /*!< Clock out at 10 MHz */
    .mode           = 0,                             /*!< SPI mode 0 */
    .spics_io_num   = PIN_DISPLAY_CS,                    /*!< CS pin */
    .queue_size     = 7,                             /*!< We want to be able to queue 7 transactions at a time */
    .pre_cb         = lcd_spi_pre_transfer_callback, /*!< Specify pre-transfer callback to handle D/C line */
  };

  /*!< Initialize the SPI bus */
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, DMA_CHAN));

  /*!< Attach the LCD to the SPI bus */
  ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &_spi));

  /**< Initialize the LCD */
  ESP_ERROR_CHECK(lcd_init(_spi));

  memset(fb, 0, sizeof(fb));
}

#endif
