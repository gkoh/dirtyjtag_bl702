/*
 * GPIO bit-bang JTAG engine for BL702 (Tang Primer 20K Dock)
 *
 * Pin assignments (Tang Primer 20K Dock):
 *   TMS = GPIO15, TCK = GPIO14, TDI = GPIO17, TDO = GPIO23
 *
 * Uses bflb_gpio HAL for all pin access.
 * Reference: firmware-bl616/fpga/programmer.cpp
 */
#include "jtag_gpio.h"
#include "bflb_gpio.h"
#include "bl702_clock.h"
#include "board/pins.h"

static struct bflb_device_s *gpio;
static volatile uint32_t jtag_delay = 0;
static uint32_t cpu_clock_hz = 0;

static inline void delay_loop(void) {
  volatile uint32_t n = jtag_delay;
  while (n--) {
    __asm__ volatile("nop");
  }
}

void jtag_gpio_init(void) {
  gpio = bflb_device_get_by_name("gpio");

  bflb_gpio_init(gpio, JTAG_TMS_PIN, GPIO_OUTPUT | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);
  bflb_gpio_init(gpio, JTAG_TCK_PIN, GPIO_OUTPUT | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);
  bflb_gpio_init(gpio, JTAG_TDI_PIN, GPIO_OUTPUT | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);
  bflb_gpio_init(gpio, JTAG_TDO_PIN, GPIO_INPUT | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);

  /* Start with TCK, TMS, TDI low */
  bflb_gpio_reset(gpio, JTAG_TCK_PIN);
  bflb_gpio_reset(gpio, JTAG_TMS_PIN);
  bflb_gpio_reset(gpio, JTAG_TDI_PIN);

  /* Cache CPU clock for frequency calculation */
  cpu_clock_hz = Clock_System_Clock_Get(BL_SYSTEM_CLOCK_FCLK);
}

void jtag_transfer(uint16_t bits, const uint8_t *tdi, uint8_t *tdo) {
  for (uint16_t i = 0; i < bits; i++) {
    uint8_t bitmask = 0x80 >> (i & 7);

    /* Set TDI */
    if (tdi[i >> 3] & bitmask) {
      bflb_gpio_set(gpio, JTAG_TDI_PIN);
    } else {
      bflb_gpio_reset(gpio, JTAG_TDI_PIN);
    }

    /* TCK low */
    bflb_gpio_reset(gpio, JTAG_TCK_PIN);
    delay_loop();

    /* TCK high */
    bflb_gpio_set(gpio, JTAG_TCK_PIN);
    delay_loop();

    /* Read TDO */
    if (tdo) {
      if (bflb_gpio_read(gpio, JTAG_TDO_PIN)) {
        tdo[i >> 3] |= bitmask;
      }
    }
  }

  /* Leave TCK low */
  bflb_gpio_reset(gpio, JTAG_TCK_PIN);
}

void jtag_set_tck(bool value) {
  if (value) {
    bflb_gpio_set(gpio, JTAG_TCK_PIN);
  } else {
    bflb_gpio_reset(gpio, JTAG_TCK_PIN);
  }
}

void jtag_set_tdi(bool value) {
  if (value) {
    bflb_gpio_set(gpio, JTAG_TDI_PIN);
  } else {
    bflb_gpio_reset(gpio, JTAG_TDI_PIN);
  }
}

void jtag_set_tms(bool value) {
  if (value) {
    bflb_gpio_set(gpio, JTAG_TMS_PIN);
  } else {
    bflb_gpio_reset(gpio, JTAG_TMS_PIN);
  }
}

void jtag_set_trst(bool value) {
  /* TRST not connected on Tang Primer 20K Dock — no-op */
  (void)value;
}

void jtag_set_srst(bool value) {
  /* SRST not connected on Tang Primer 20K Dock — no-op */
  (void)value;
}

bool jtag_get_tdo(void) {
  return bflb_gpio_read(gpio, JTAG_TDO_PIN);
}

bool jtag_strobe(uint8_t pulses, bool tms, bool tdi) {
  if (tms) {
    bflb_gpio_set(gpio, JTAG_TMS_PIN);
  } else {
    bflb_gpio_reset(gpio, JTAG_TMS_PIN);
  }
  if (tdi) {
    bflb_gpio_set(gpio, JTAG_TDI_PIN);
  } else {
    bflb_gpio_reset(gpio, JTAG_TDI_PIN);
  }

  bool last_tdo = false;
  for (uint8_t i = 0; i < pulses; i++) {
    bflb_gpio_reset(gpio, JTAG_TCK_PIN);
    delay_loop();
    bflb_gpio_set(gpio, JTAG_TCK_PIN);
    delay_loop();
    last_tdo = bflb_gpio_read(gpio, JTAG_TDO_PIN);
  }
  bflb_gpio_reset(gpio, JTAG_TCK_PIN);

  return last_tdo;
}

void jtag_set_frequency(uint16_t kHz) {
  if (kHz == 0) {
    jtag_delay = 0;
    return;
  }
  /*
   * Each delay iteration ~3 cycles (loop + nop).
   * For frequency f: half-period = 1/(2f), cycles = cpu_clock / (2*f*1000) / 3
   * Use cached CPU clock (144 MHz on BL702, not the BL616's 320 MHz).
   */
  uint32_t target = (cpu_clock_hz / 1000) / (2 * (uint32_t)kHz * 3);
  jtag_delay = (target > 1) ? target : 0;
}
