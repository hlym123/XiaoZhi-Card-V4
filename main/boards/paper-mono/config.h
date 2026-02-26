#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#ifdef __cplusplus
#include "M5IOE1.h"
#include "M5PM1.h"
#endif

/*
 * PaperMono 硬件配置
 * 参考: ~/m5stack_dev/product/others/PaperMono/develop/src/components/m5stack_paper_mono/README.md
 * MCU: ESP32-S3, Display: 480x800 SPI EPD GDEM0397T81P
 */

/* ---------------------------------------------------------------- */
// System I2C (I2C1)
#define SYS_I2C_NUM     I2C_NUM_1
#define SYS_I2C_PIN_SCL GPIO_NUM_48
#define SYS_I2C_PIN_SDA GPIO_NUM_47

/* ---------------------------------------------------------------- */
// EPD Display (GDEM0397T81P 480x800)
#define EPD_SPI_HOST   SPI2_HOST
#define EPD_BUSY_PIN   GPIO_NUM_18
#define EPD_PIN_SCK    GPIO_NUM_15
#define EPD_PIN_MOSI   GPIO_NUM_14
#define EPD_PIN_MISO   GPIO_NUM_NC
#define EPD_PIN_DC     GPIO_NUM_17
#define EPD_PIN_CS     GPIO_NUM_16

/* M5IOE1 pins for EPD */
#define EPD_PWR_EN_IOE_PIN  M5IOE1_PIN_3   /* PWR_EN, active high */
#define EPD_RST_IOE_PIN     M5IOE1_PIN_5   /* RST */

/* ---------------------------------------------------------------- */
// Touch (FT5X06)
#define TOUCH_INT_GPIO     GPIO_NUM_4
#define TP_PWR_EN_IOE_PIN  M5IOE1_PIN_13  /* PWR_EN, active high */
#define TP_RST_IOE_PIN     M5IOE1_PIN_6   /* RST */

/* ---------------------------------------------------------------- */
// Backlight (M5PM1 GPIO3 = PWM0)
#define BL_PWM_GPIO  M5PM1_GPIO_NUM_3
#define BL_PWM_CH    M5PM1_PWM_CH_0

/* ---------------------------------------------------------------- */
// PDM Microphone (LMD4737T261), I2S0
#define PDM_CLK_PIN         GPIO_NUM_45
#define PDM_DATA_PIN        GPIO_NUM_46
#define PDM_PWR_EN_IOE_PIN  M5IOE1_PIN_12  /* Power, active high */

/* ---------------------------------------------------------------- */
// Display (480x800)
#define EPD_RES_WIDTH       480
#define EPD_RES_HEIGHT      800
#define DISPLAY_WIDTH       EPD_RES_WIDTH
#define DISPLAY_HEIGHT      EPD_RES_HEIGHT
#define DISPLAY_MIRROR_X    false
#define DISPLAY_MIRROR_Y    false
#define DISPLAY_SWAP_XY     false
#define DISPLAY_OFFSET_X    0
#define DISPLAY_OFFSET_Y    0

/* ---------------------------------------------------------------- */
// 按键
#define GPIO_KEY1           GPIO_NUM_2
#define GPIO_KEY2           GPIO_NUM_3
#define USER_BUTTON_GPIO    GPIO_NUM_2

/* ---------------------------------------------------------------- */
// 其他
#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC


#endif  // _BOARD_CONFIG_H_
