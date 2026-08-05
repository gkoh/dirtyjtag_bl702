#include "bflb_clock.h"
#include "bflb_ef_ctrl.h"
#include "bflb_flash.h"
#include "bflb_gpio.h"
#include "bflb_rtc.h"
#include "bflb_sec_mutex.h"
#include "bflb_uart.h"

#include "bl702_glb.h"
#include "board.h"
#include "ef_data_reg.h"

#include "mm.h"

extern void log_start(void);

extern uint32_t __HeapBase;
extern uint32_t __HeapLimit;

#if defined(CONFIG_BFLB_LOG)
static struct bflb_device_s *rtc;
#endif

static struct bflb_device_s *console_uart;

static void console_init(void) {
  struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");

  /* UART0 TX=GPIO_14, RX=GPIO_23 (J2 header, per dock schematic) */
  bflb_gpio_uart_init(gpio, GPIO_PIN_14, GPIO_UART_FUNC_UART0_TX);
  bflb_gpio_uart_init(gpio, GPIO_PIN_23, GPIO_UART_FUNC_UART0_RX);

  console_uart = bflb_device_get_by_name("uart0");

  struct bflb_uart_config_s cfg = {
      .baudrate = 2000000,
      .data_bits = UART_DATA_BITS_8,
      .stop_bits = UART_STOP_BITS_1,
      .parity = UART_PARITY_NONE,
      .tx_fifo_threshold = 7,
      .rx_fifo_threshold = 7,
      .bit_order = UART_LSB_FIRST,
      .flow_ctrl = 0,
  };
  bflb_uart_init(console_uart, &cfg);
  bflb_uart_set_console(console_uart);
}

static void system_clock_init(void) {
  GLB_Set_System_CLK(GLB_DLL_XTAL_32M, GLB_SYS_CLK_DLL144M);
  GLB_Set_MTimer_CLK(1, GLB_MTIMER_CLK_BCLK, 71);
  HBN_Set_XCLK_CLK_Sel(HBN_XCLK_CLK_XTAL);
}

static void peripheral_clock_init(void) {
  PERIPHERAL_CLOCK_ADC_DAC_ENABLE();
  PERIPHERAL_CLOCK_SEC_ENABLE();
  PERIPHERAL_CLOCK_DMA0_ENABLE();
  PERIPHERAL_CLOCK_UART0_ENABLE();
  PERIPHERAL_CLOCK_UART1_ENABLE();
  PERIPHERAL_CLOCK_SPI0_ENABLE();
  PERIPHERAL_CLOCK_I2C0_ENABLE();
  PERIPHERAL_CLOCK_PWM0_ENABLE();
  PERIPHERAL_CLOCK_TIMER0_1_WDG_ENABLE();
  PERIPHERAL_CLOCK_IR_ENABLE();
  PERIPHERAL_CLOCK_I2S_ENABLE();
  PERIPHERAL_CLOCK_USB_ENABLE();
  GLB_AHB_Slave1_Clock_Gate(DISABLE, BL_AHB_SLAVE1_CAM);

  GLB_Set_UART_CLK(ENABLE, HBN_UART_CLK_96M, 0);
  GLB_Set_SPI_CLK(ENABLE, 0);
  GLB_Set_I2C_CLK(ENABLE, 0);
  GLB_Set_IR_CLK(ENABLE, GLB_IR_CLK_SRC_XCLK, 15);

  GLB_Set_ADC_CLK(ENABLE, GLB_ADC_CLK_XCLK, 1);
  GLB_Set_DAC_CLK(ENABLE, GLB_DAC_CLK_XCLK, 0x3E);

  GLB_Set_USB_CLK(ENABLE);
}

void bl_show_log(void) {
  printf("\r\n");
  printf("  ____               __  __      _       _       _     \r\n");
  printf(" |  _ \\             / _|/ _|    | |     | |     | |    \r\n");
  printf(" | |_) | ___  _   _| |_| |_ __ _| | ___ | | __ _| |__  \r\n");
  printf(" |  _ < / _ \\| | | |  _|  _/ _` | |/ _ \\| |/ _` | '_ \\ \r\n");
  printf(" | |_) | (_) | |_| | | | || (_| | | (_) | | (_| | |_) |\r\n");
  printf(" |____/ \\___/ \\__,_|_| |_| \\__,_|_|\\___/|_|\\__,_|.__/ \r\n");
  printf("\r\n");
  printf("Build:%s,%s\r\n", __TIME__, __DATE__);
  printf("DirtyJTAG BL702 — Tang Primer 20K Dock\r\n");
}

void bl_show_flashinfo(void) {
  spi_flash_cfg_type flashCfg;
  uint8_t *pFlashCfg = NULL;
  uint32_t flashSize = 0;
  uint32_t flashCfgLen = 0;
  uint32_t flashJedecId = 0;

  flashJedecId = bflb_flash_get_jedec_id();
  flashSize = bflb_flash_get_size();
  bflb_flash_get_cfg(&pFlashCfg, &flashCfgLen);
  arch_memcpy((void *)&flashCfg, pFlashCfg, flashCfgLen);
  printf("======== flash cfg ========\r\n");
  printf("flash size 0x%08X\r\n", flashSize);
  printf("jedec id     0x%06X\r\n", flashJedecId);
  printf("mid              0x%02X\r\n", flashCfg.mid);
  printf("iomode           0x%02X\r\n", flashCfg.io_mode);
  printf("clk delay        0x%02X\r\n", flashCfg.clk_delay);
  printf("clk invert       0x%02X\r\n", flashCfg.clk_invert);
  printf("read reg cmd0    0x%02X\r\n", flashCfg.read_reg_cmd[0]);
  printf("read reg cmd1    0x%02X\r\n", flashCfg.read_reg_cmd[1]);
  printf("write reg cmd0   0x%02X\r\n", flashCfg.write_reg_cmd[0]);
  printf("write reg cmd1   0x%02X\r\n", flashCfg.write_reg_cmd[1]);
  printf("qe write len     0x%02X\r\n", flashCfg.qe_write_reg_len);
  printf("cread support    0x%02X\r\n", flashCfg.c_read_support);
  printf("cread code       0x%02X\r\n", flashCfg.c_read_mode);
  printf("burst wrap cmd   0x%02X\r\n", flashCfg.burst_wrap_cmd);
  printf("===========================\r\n");
}

void board_recovery(void) {
  system_clock_init();
  peripheral_clock_init();
}

void ram_heap_init(void) {
  size_t heap_len;

  mem_manager_init();

  heap_len = ((size_t)&__HeapLimit - (size_t)&__HeapBase);
  mm_register_heap(MM_HEAP_OCRAM_0, "OCRAM", MM_ALLOCATOR_TLSF, &__HeapBase, heap_len);

  printf(
      "dynamic memory init success\r\n"
      "  ocram heap size: %d Kbyte \r\n",
      ((size_t)&__HeapLimit - (size_t)&__HeapBase) / 1024);
}

void board_init(void) {
  int ret = -1;
  uintptr_t flag;

  flag = bflb_irq_save();

  ret = bflb_flash_init();

  system_clock_init();
  peripheral_clock_init();
  bflb_irq_initialize();

  console_init();

  ram_heap_init();

  bl_show_log();
  if (ret != 0) {
    printf("flash init fail!!!\r\n");
  }
  bl_show_flashinfo();

  printf("cgen1:%08x\r\n", getreg32(BFLB_GLB_CGEN1_BASE));

  log_start();
#if defined(CONFIG_BFLB_LOG)
  rtc = bflb_device_get_by_name("rtc");
#endif

  bflb_sec_mutex_init();

  bflb_irq_restore(flag);

  printf("board init done\r\n");
  printf("===========================\r\n");
}

#ifdef CONFIG_BFLB_LOG
__attribute__((weak)) uint64_t bflb_log_clock(void) {
  return bflb_mtimer_get_time_us();
}

__attribute__((weak)) uint32_t bflb_log_time(void) {
  return BFLB_RTC_TIME2SEC(bflb_rtc_get_time(rtc));
}

__attribute__((weak)) char *bflb_log_thread(void) {
  return "";
}
#endif
