#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include <stdarg.h>
#include "port_macros.h"
#include "timers.h"
#include "spcr.h"

#include "teensy.h"
#include "usb_serial.h"

#define USB_LOG_MAX_LEN  64
#include "usb_log.h"

#define HEADER_SIZE 4

#define R_MAX = 63;
#define G_MAX = 63;
#define B_MAX = 63;
#define R_MIN = 0;
#define G_MIN = 0;
#define B_MIN = 0;

#define PIXELS      20

// stored as [1rrrrrgg] [gggbbbbb]
static volatile uint16_t buffer1[PIXELS];
static volatile uint16_t buffer2[PIXELS];

static volatile uint16_t *xmit = buffer1;
static volatile uint16_t *draw = buffer2;

static volatile uint8_t updating = 1;
static volatile uint8_t header = 0;
static volatile uint8_t pixel = 0;


void update(void);


void set_color(uint8_t pixel, uint16_t color);
void set_rgb(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b);

inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t color = r << 5;
  color |= (g & 0x1f);
  color <<= 5;
  color |= (b & 0x1f);
  return color;
}

void init_buffer(volatile uint16_t *b) {
  for (int i = 0; i < PIXELS; i ++) {
    b[i] = 0x8000;
  }
}

void init_usb() {
}

void init_spi() {
  OUTPUT(SS);
  OUTPUT(SCLK);
  OUTPUT(MOSI);
  INPUT(MISO);
  SET(SS);
  SPCR = _BV(SPE) | _BV(SPIE) | _BV(MSTR) | SPI_CLK_SCALE_2;
  sei();
  // start things rolling
  SPDR = 0;
}
#define SPI_ENABLE  SPCR |= _BV(SPE);
#define SPI_DISABLE SPCR &= ~_BV(SPE);

//static char
//snprintf(text, 20, "%d", RED(color));
//char *__s, size_t __n, const char *__fmt, ...

void set_color(uint8_t pixel, uint16_t color) {
  draw[pixel] = color;
}

void update() {
  while (updating) {}

  // swap buffers
  volatile uint16_t *tmp = xmit;
  xmit = draw;
  draw = tmp;

  header = 0;
  pixel = 0;
  updating = 1;
}

ISR (SPI_STC_vect) {
  static uint16_t color_value = 0;
  static uint8_t byte = 0;
  if (!updating) {
    SPDR = 0;
  } else if (header < HEADER_SIZE) {
      SPDR = 0;
      header++;
  } else if (pixel < PIXELS) {
    if (byte == 0) {
      color_value = xmit[pixel];
      SPDR = (0x80 | ((color_value >> 3) & 0x7f));
      byte = 1;
    } else {
      SPDR = (((color_value >> 10) & 0x1f) | ((color_value << 5) & 0xe0));
      byte = 0;
      pixel++;
    }
  } else {
    updating = 0;
    SPDR = 0;
  }
}

int main(void) {
  clock_prescale_set(0);
  set_sleep_mode(SLEEP_MODE_IDLE);
  init_spi();
  init_buffer(buffer1);
  init_buffer(buffer2);

  uint8_t phase = 0;
  uint8_t gshift = 0;
  uint8_t bshift = 0;
  for (;;) {
    update();
    _delay_ms(33);
    // Symmetric linear color sweep
    // Up
    for (uint8_t i = 0; i < 32; i++) {
      uint8_t pos = (i + phase) % 64;
      if (pos < PIXELS) {
        set_color(pos, rgb(i, (i + gshift) % 64, (i + bshift) % 64));
      }
    }
    // Down
    for (uint8_t i = 32; i < 64; i++) {
      uint8_t pos = (i + phase) % 64;
      if (pos < PIXELS) {
        set_color(pos, rgb(i, (i + gshift) % 64, (i + bshift) % 64));
      }
    }
    phase++;
    gshift+=3;
    bshift+=2;
  }
}
