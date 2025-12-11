
#if defined(CONFIG_BOARD_FRDM_MCXN947) || \
    defined(CONFIG_BOARD_FRDM_MCXN947_MCXN947_CPU0)

/* NXP FRDM-MCXN947 Configuration */
/* CS: P0_27 (Arduino D10) - Note: Also Green LED */
#define DISPLAY_PIN_CS_N 27
/* PDL: P0_28 (Arduino D8) */
#define DISPLAY_PIN_PD_N 28

/* Dummy definitions for unused pins or pins handled by Zephyr drivers */
#define DISPLAY_PIN_SCK 0
#define DISPLAY_PIN_MISO 0
#define DISPLAY_PIN_MOSI 0

/* Buttons (SW2=P0_23, SW3=P0_6) */
#define BUTTON_PIN_1 23
#define BUTTON_PIN_2 6
#define BUTTON_PIN_3 0
#define BUTTON_PIN_4 0

/* LEDs (Red=P0_10, Green=P0_27, Blue=P1_2) - Check schematic, using
 * placeholders based on dts */
/* Green LED: P0_27 (0x1b) */
#define LED_PIN_1 27
/* Blue LED: P1_2 (0x2) - Note: This is on GPIO1, code using gpio0_dev will fail
 * if not updated */
#define LED_PIN_2 2
/* Red LED: P0_10 (0xa) */
#define LED_PIN_3 10
#define LED_PIN_4 0

#else

#include "hal/nrf_gpio.h"

#define NRF_UARTE NRF_UARTE1_S

#define BUTTON_PIN_1 23
#define BUTTON_PIN_2 24
#define BUTTON_PIN_3 8
#define BUTTON_PIN_4 9

#define LED_PIN_1 28
#define LED_PIN_2 29
#define LED_PIN_3 30
#define LED_PIN_4 31

#define UART_TX_PIN 20
#define UART_RX_PIN 22

#define QSPI_SCK_PIN 17
#define QSPI_CSN_PIN 18
#define QSPI_IO0_PIN 13
#define QSPI_IO1_PIN 14
#define QSPI_IO2_PIN 15
#define QSPI_IO3_PIN 16

#define SDC_SCK_PIN NRF_GPIO_PIN_MAP(1, 14)
#define SDC_MOSI_PIN NRF_GPIO_PIN_MAP(1, 13)
#define SDC_MISO_PIN NRF_GPIO_PIN_MAP(1, 15)
#define SDC_CS_PIN NRF_GPIO_PIN_MAP(1, 12)

#define DISPLAY_PIN_SCK NRF_GPIO_PIN_MAP(0, 6)
#define DISPLAY_PIN_MISO NRF_GPIO_PIN_MAP(0, 5)
#define DISPLAY_PIN_MOSI NRF_GPIO_PIN_MAP(0, 25)
#define DISPLAY_PIN_CS_N NRF_GPIO_PIN_MAP(0, 7)
#define DISPLAY_PIN_PD_N 26

// #define MAX98357
#define PCM5102

#ifdef PCM5102
#define I2S_PIN_SCK NRF_GPIO_PIN_MAP(1, 9)
#define I2S_PIN_BCK NRF_GPIO_PIN_MAP(1, 8)
#define I2S_PIN_DIN NRF_GPIO_PIN_MAP(1, 7)
#define I2S_PIN_LRCK NRF_GPIO_PIN_MAP(1, 6)
#endif

#ifdef MAX98357
#define I2S_PIN_SD NRF_GPIO_PIN_MAP(0, 10);
#define I2S_PIN_GAIN NRF_GPIO_PIN_MAP(0, 9);
#define I2S_PIN_DIN NRF_GPIO_PIN_MAP(1, 0);
#define I2S_PIN_BCK NRF_GPIO_PIN_MAP(0, 24);   // BCLK
#define I2S_PIN_LRCK NRF_GPIO_PIN_MAP(0, 22);  // LRC
#endif

#endif
