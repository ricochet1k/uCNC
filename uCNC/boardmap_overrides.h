#ifndef BOADMAP_OVERRIDES_H
#define BOADMAP_OVERRIDES_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "boardmap_reset.h"
#define MCU MCU_LPC176X
#define KINEMATIC KINEMATIC_CARTESIAN
#define AXIS_COUNT 5
#define TOOL_COUNT 1
#define BAUDRATE 115200
#define BOARD_NAME "SKR v1.4"
#define UART_PORT 3
#define UART2_PORT 0
#define SPI_PORT 0
#define SPI2_PORT 1
#define I2C_PORT 1
#define ITP_TIMER 0
#define SERVO_TIMER 1
#define ONESHOT_TIMER 2
#define STEP0_BIT 2
#define STEP0_PORT 2
#define STEP1_BIT 19
#define STEP1_PORT 0
#define STEP2_BIT 22
#define STEP2_PORT 0
#define STEP3_BIT 13
#define STEP3_PORT 2
#define STEP4_BIT 15
#define STEP4_PORT 1
#define DIR0_BIT 6
#define DIR0_PORT 2
#define DIR1_BIT 20
#define DIR1_PORT 0
#define DIR2_BIT 11
#define DIR2_PORT 2
#define DIR3_BIT 11
#define DIR3_PORT 0
#define DIR4_BIT 14
#define DIR4_PORT 1
#define STEP0_EN_BIT 1
#define STEP0_EN_PORT 2
#define STEP1_EN_BIT 8
#define STEP1_EN_PORT 2
#define STEP2_EN_BIT 21
#define STEP2_EN_PORT 0
#define STEP3_EN_BIT 12
#define STEP3_EN_PORT 2
#define STEP4_EN_BIT 16
#define STEP4_EN_PORT 1
#define PWM0_BIT 5
#define PWM0_PORT 2
#define PWM0_CHANNEL 6
#define PWM1_BIT 4
#define PWM1_PORT 2
#define PWM1_CHANNEL 5
#define PWM2_BIT 3
#define PWM2_PORT 2
#define PWM2_CHANNEL 4
#define SERVO0_BIT 20
#define SERVO0_PORT 1
#define SERVO1_BIT 21
#define SERVO1_PORT 1
#define SERVO2_BIT 19
#define SERVO2_PORT 1
#define SERVO3_BIT 18
#define SERVO3_PORT 1
#define DOUT4_BIT 20
#define DOUT4_PORT 1
#define DOUT5_BIT 18
#define DOUT5_PORT 1
#define DOUT6_BIT 19
#define DOUT6_PORT 1
#define DOUT7_BIT 30
#define DOUT7_PORT 1
#define DOUT19_BIT 6
#define DOUT19_PORT 0
#define DOUT20_BIT 10
#define DOUT20_PORT 1
#define DOUT21_BIT 9
#define DOUT21_PORT 1
#define DOUT22_BIT 8
#define DOUT22_PORT 1
#define DOUT23_BIT 4
#define DOUT23_PORT 1
#define DOUT24_BIT 1
#define DOUT24_PORT 1
#define DOUT27_BIT 28
#define DOUT27_PORT 4
#define DOUT29_BIT 17
#define DOUT29_PORT 1
#define DOUT30_BIT 4
#define DOUT30_PORT 0
#define DOUT31_BIT 18
#define DOUT31_PORT 1
#define LIMIT_B_BIT 25
#define LIMIT_B_PORT 1
#define PROBE_BIT 10
#define PROBE_PORT 0
#define PROBE_PULLUP
#define ANALOG0_BIT 23
#define ANALOG0_PORT 0
#define ANALOG0_CHANNEL 0
#define ANALOG1_BIT 24
#define ANALOG1_PORT 0
#define ANALOG1_CHANNEL 1
#define ANALOG2_BIT 25
#define ANALOG2_PORT 0
#define ANALOG2_CHANNEL 2
#define ANALOG3_BIT 26
#define ANALOG3_PORT 0
#define ANALOG3_CHANNEL 3
#define DIN16_BIT 28
#define DIN16_PORT 0
#define DIN16_PULLUP
#define DIN17_BIT 25
#define DIN17_PORT 3
#define DIN17_PULLUP
#define DIN18_BIT 26
#define DIN18_PORT 3
#define DIN18_PULLUP
#define DIN19_BIT 27
#define DIN19_PORT 0
#define DIN19_PULLUP
#define DIN20_BIT 10
#define DIN20_PORT 1
#define DIN20_PULLUP
#define DIN21_BIT 9
#define DIN21_PORT 1
#define DIN21_PULLUP
#define DIN22_BIT 8
#define DIN22_PORT 1
#define DIN22_PULLUP
#define DIN23_BIT 4
#define DIN23_PORT 1
#define DIN23_PULLUP
#define DIN24_BIT 1
#define DIN24_PORT 1
#define DIN24_PULLUP
#define DIN27_BIT 29
#define DIN27_PORT 4
#define DIN29_BIT 5
#define DIN29_PORT 0
#define TX_BIT 28
#define TX_PORT 4
#define RX_BIT 29
#define RX_PORT 4
#define USB_DM_BIT 30
#define USB_DM_PORT 0
#define USB_DP_BIT 29
#define USB_DP_PORT 0
#define SPI_CLK_BIT 15
#define SPI_CLK_PORT 0
#define SPI_SDI_BIT 17
#define SPI_SDI_PORT 0
#define SPI_SDO_BIT 18
#define SPI_SDO_PORT 0
#define SPI_CS_BIT 16
#define SPI_CS_PORT 0
#define I2C_CLK_BIT 1
#define I2C_CLK_PORT 0
#define I2C_DATA_BIT 0
#define I2C_DATA_PORT 0
#define SPI2_CLK_BIT 7
#define SPI2_CLK_PORT 0
#define SPI2_SDI_BIT 8
#define SPI2_SDI_PORT 0
#define SPI2_SDO_BIT 9
#define SPI2_SDO_PORT 0
#define SPI2_CS_BIT 6
#define SPI2_CS_PORT 0
//Custom configurations


#ifdef __cplusplus
}
#endif
#endif
