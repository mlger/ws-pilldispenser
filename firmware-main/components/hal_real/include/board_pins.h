#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/*
 * Waveshare ESP32-S3-Touch-LCD-4.3B pin map (wiki "Interface Description"
 * tables + official demo; verified against docs.waveshare.com 2026-10).
 */

/* Shared I2C bus (touch, CH422G IO expander, PCF85063 RTC). */
#define PD_PIN_I2C_SDA 8
#define PD_PIN_I2C_SCL 9
#define PD_I2C_FREQ_HZ 400000

/* GT911 touch. */
#define PD_PIN_TP_IRQ 4

/* RGB LCD (ESP32-S3 LCD_CAM, ST7262 panel — VERIFY panel IC on teardown). */
#define PD_PIN_RGB_VSYNC 3
#define PD_PIN_RGB_HSYNC 46
#define PD_PIN_RGB_DE    5
#define PD_PIN_RGB_PCLK  7
/* RGB565 data bus D0..D15. */
#define PD_PIN_RGB_DATA0  14
#define PD_PIN_RGB_DATA1  38
#define PD_PIN_RGB_DATA2  18
#define PD_PIN_RGB_DATA3  17
#define PD_PIN_RGB_DATA4  10
#define PD_PIN_RGB_DATA5  39
#define PD_PIN_RGB_DATA6  0
#define PD_PIN_RGB_DATA7  45
#define PD_PIN_RGB_DATA8  48
#define PD_PIN_RGB_DATA9  47
#define PD_PIN_RGB_DATA10 21
#define PD_PIN_RGB_DATA11 1
#define PD_PIN_RGB_DATA12 2
#define PD_PIN_RGB_DATA13 42
#define PD_PIN_RGB_DATA14 41
#define PD_PIN_RGB_DATA15 40

/* ST7262 800x480 timing (Waveshare demo, 16 MHz pixel clock). */
#define PD_LCD_PCLK_HZ  (16 * 1000 * 1000)
#define PD_LCD_HSYNC_PW 4
#define PD_LCD_HSYNC_BP 8
#define PD_LCD_HSYNC_FP 8
#define PD_LCD_VSYNC_PW 4
#define PD_LCD_VSYNC_BP 8
#define PD_LCD_VSYNC_FP 8

/* RS485 link to the dispense-mechanism co-processor (auto-direction
 * transceiver; shares no pins when the console is on USB-Serial-JTAG). */
#define PD_PIN_RS485_TXD  44
#define PD_PIN_RS485_RXD  43
#define PD_RS485_BAUD     115200
#define PD_RS485_UART_NUM 1

/* TF card SPI (CS is CH422G EXIO4 — see hal_storage_ext.h). */
#define PD_PIN_TF_MOSI 11
#define PD_PIN_TF_SCK  12
#define PD_PIN_TF_MISO 13

#endif /* BOARD_PINS_H */
