/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "boards.h"

#include "esp_rom_gpio.h"
#include "hal/gpio_ll.h"
#include "hal/usb_hal.h"
#include "soc/usb_periph.h"

#include "driver/periph_ctrl.h"
#include "driver/rmt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

#ifdef PIN_NEOPIXEL
#include "led_strip.h"
static led_strip_t *strip;
#endif

#ifdef PIN_APA102_SCK
#include "led_strip_spi_apa102.h"
#endif

#define RGB_USB_UNMOUNTED   0xff, 0x00, 0x00 // Red
#define RGB_USB_MOUNTED     0x00, 0xff, 0x00 // Green
#define RGB_WRITING         0xcc, 0x66, 0x00
#define RGB_UNKNOWN         0x00, 0x00, 0x88 // for debug
#define RGB_BLACK           0x00, 0x00, 0x00 // clear

static void configure_pins(usb_hal_context_t *usb);

void board_init(void)
{

#ifdef PIN_LED
//#define BLINK_GPIO 26
//  gpio_pad_select_gpio(BLINK_GPIO);
//  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
//  gpio_set_drive_capability(BLINK_GPIO, GPIO_DRIVE_CAP_3);
//  gpio_set_level(BLINK_GPIO, 1);
#endif

#ifdef PIN_NEOPIXEL
  // WS2812 Neopixel driver with RMT peripheral
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX(PIN_NEOPIXEL, RMT_CHANNEL_0);
  config.clk_div = 2; // set counter clock to 40MHz

  rmt_config(&config);
  rmt_driver_install(config.channel, 0, 0);

  led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(NEOPIXEL_NUMBER, (led_strip_dev_t) config.channel);
  strip = led_strip_new_rmt_ws2812(&strip_config);
  strip->clear(strip, 100); // off led
  strip->set_brightness(strip, NEOPIXEL_BRIGHTNESS);
#endif

#ifdef PIN_APA102_SCK
    // Setup the IO for the APA DATA and CLK
    gpio_pad_select_gpio(PIN_APA102_DATA);
    gpio_pad_select_gpio(PIN_APA102_SCK);
    gpio_ll_input_disable(&GPIO, PIN_APA102_DATA);
    gpio_ll_input_disable(&GPIO, PIN_APA102_SCK);
    gpio_ll_output_enable(&GPIO, PIN_APA102_DATA);
    gpio_ll_output_enable(&GPIO, PIN_APA102_SCK);

    // Initialise SPI
    setupSPI(PIN_APA102_DATA, PIN_APA102_SCK);

    // Initialise the APA
    initAPA(APA102_BRIGHTNESS);
#endif

  // USB Controller Hal init
  periph_module_reset(PERIPH_USB_MODULE);
  periph_module_enable(PERIPH_USB_MODULE);

  usb_hal_context_t hal = {
    .use_external_phy = false // use built-in PHY
  };
  usb_hal_init(&hal);
  configure_pins(&hal);
}

void board_teardown(void)
{

}

static void configure_pins(usb_hal_context_t *usb)
{
  /* usb_periph_iopins currently configures USB_OTG as USB Device.
   * Introduce additional parameters in usb_hal_context_t when adding support
   * for USB Host.
   */
  for (const usb_iopin_dsc_t *iopin = usb_periph_iopins; iopin->pin != -1; ++iopin) {
    if ((usb->use_external_phy) || (iopin->ext_phy_only == 0)) {
      esp_rom_gpio_pad_select_gpio(iopin->pin);
      if (iopin->is_output) {
        esp_rom_gpio_connect_out_signal(iopin->pin, iopin->func, false, false);
      } else {
        esp_rom_gpio_connect_in_signal(iopin->pin, iopin->func, false);
        if ((iopin->pin != GPIO_FUNC_IN_LOW) && (iopin->pin != GPIO_FUNC_IN_HIGH)) {
          gpio_ll_input_enable(&GPIO, iopin->pin);
        }
      }
      esp_rom_gpio_pad_unhold(iopin->pin);
    }
  }
  if (!usb->use_external_phy) {
    gpio_set_drive_capability(USBPHY_DM_NUM, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(USBPHY_DP_NUM, GPIO_DRIVE_CAP_3);
  }
}

//--------------------------------------------------------------------+
// LED pattern
//--------------------------------------------------------------------+



#ifdef PIN_NEOPIXEL
static void neopixel_set(uint8_t r, uint8_t g, uint8_t b)
{
    if ( r+g+b == 0 )
    {
        // we cant to clear instead of setting black
        strip->clear(strip, 100);
    }
    else
    {
        strip->set_pixel(strip, 0, r, g, b);
        strip->refresh(strip, 100);
    }
}
#endif

// Common wrapper for setting pixels per type
void set_pixel(uint8_t r, uint8_t g, uint8_t b)
{
#ifdef PIN_NEOPIXEL
    neopixel_set(r, g, b);
#endif

#ifdef PIN_APA102_SCK
    setAPA(r, g, b);
#endif
}

#if defined PIN_NEOPIXEL || defined PIN_APA102_SCK
TimerHandle_t blinky_tm = NULL;

void led_blinky_cb(TimerHandle_t xTimer)
{
  (void) xTimer;
  static bool led_state = false;
  led_state = 1 - led_state; // toggle

  if ( led_state )
  {
    set_pixel(RGB_WRITING);
  }else
  {
    set_pixel(RGB_BLACK);
  }
}
#endif


void board_led_state(uint32_t state)
{
  #if defined PIN_NEOPIXEL || defined PIN_APA102_SCK
  switch(state)
  {
    case STATE_BOOTLOADER_STARTED:
    case STATE_USB_UNMOUNTED:
      set_pixel(RGB_USB_UNMOUNTED);
    break;

    case STATE_USB_MOUNTED:
      set_pixel(RGB_USB_MOUNTED);
    break;

    case STATE_WRITING_STARTED:
      // soft timer for blinky
      blinky_tm = xTimerCreate(NULL, pdMS_TO_TICKS(50), true, NULL, led_blinky_cb);
      xTimerStart(blinky_tm, 0);
    break;

    case STATE_WRITING_FINISHED:
      xTimerStop(blinky_tm, 0);
      set_pixel(RGB_WRITING);
    break;

    default:
      set_pixel(RGB_UNKNOWN);
    break;
  }
  #endif
}