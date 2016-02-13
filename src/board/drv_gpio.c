#include <stdint.h>
#include "../sensors.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "stm32f10x_conf.h"
#include "core_cm3.h"
#include "printf.h"
#include "drv_system.h"         // timers, delays, etc
#include "drv_gpio.h"
#include "utils.h"

#define I2C_DEVICE (I2CDEV_2)

typedef void (*serialReceiveCallbackPtr)(uint16_t data);   // used by serial drivers to return frames to app
typedef uint16_t (*rcReadRawDataPtr)(uint8_t chan);        // used by receiver driver to return channel data
typedef void (*pidControllerFuncPtr)(void);                // pid controller function prototype

#define GYRO
#define ACC
#define BUZZER
#define LED0
#define LED1
#define INVERTER

#define I2C_DEVICE (I2CDEV_2)

#include "drv_adc.h"
#include "drv_i2c.h"
#include "drv_spi.h"
#include "drv_mpu6050.h"
#include "drv_pwm.h"
#include "drv_timer.h"
#include "drv_serial.h"
#include "drv_uart.h"

void gpioInit(GPIO_TypeDef *gpio, gpio_config_t *config)
{
    uint32_t pinpos;
    for (pinpos = 0; pinpos < 16; pinpos++) {
        // are we doing this pin?
        if (config->pin & (0x1 << pinpos)) {
            // reference CRL or CRH, depending whether pin number is 0..7 or 8..15
            __IO uint32_t *cr = &gpio->CRL + (pinpos / 8);
            // mask out extra bits from pinmode, leaving just CNF+MODE
            uint32_t currentmode = config->mode & 0x0F;
            // offset to CNF and MODE portions of CRx register
            uint32_t shift = (pinpos % 8) * 4;
            // Read out current CRx value
            uint32_t tmp = *cr;
            // if we're in output mode, add speed too.
            if (config->mode & 0x10)
                currentmode |= config->speed;
            // Mask out 4 bits
            tmp &= ~(0xF << shift);
            // apply current pinmode
            tmp |= currentmode << shift;
            *cr = tmp;
            // Special handling for IPD/IPU
            if (config->mode == Mode_IPD) {
                gpio->ODR &= ~(1U << pinpos);
            } else if (config->mode == Mode_IPU) {
                gpio->ODR |= (1U << pinpos);
            }
        }
    }
}

void gpioExtiLineConfig(uint8_t portsrc, uint8_t pinsrc)
{
    uint32_t tmp = 0x00;

    tmp = ((uint32_t)0x0F) << (0x04 * (pinsrc & (uint8_t)0x03));
    AFIO->EXTICR[pinsrc >> 0x02] &= ~tmp;
    AFIO->EXTICR[pinsrc >> 0x02] |= (((uint32_t)portsrc) << (0x04 * (pinsrc & (uint8_t)0x03)));
}

#define LSB_MASK                    ((uint16_t)0xFFFF)
#define DBGAFR_POSITION_MASK        ((uint32_t)0x000F0000)
#define DBGAFR_SWJCFG_MASK          ((uint32_t)0xF0FFFFFF)
#define DBGAFR_LOCATION_MASK        ((uint32_t)0x00200000)
#define DBGAFR_NUMBITS_MASK         ((uint32_t)0x00100000)

void gpioPinRemapConfig(uint32_t remap, bool enable)
{
    uint32_t tmp = 0x00, tmp1 = 0x00, tmpreg = 0x00, tmpmask = 0x00;
    if ((remap & 0x80000000) == 0x80000000)
        tmpreg = AFIO->MAPR2;
    else
        tmpreg = AFIO->MAPR;

    tmpmask = (remap & DBGAFR_POSITION_MASK) >> 0x10;
    tmp = remap & LSB_MASK;

    if ((remap & (DBGAFR_LOCATION_MASK | DBGAFR_NUMBITS_MASK)) == (DBGAFR_LOCATION_MASK | DBGAFR_NUMBITS_MASK)) {
        tmpreg &= DBGAFR_SWJCFG_MASK;
        AFIO->MAPR &= DBGAFR_SWJCFG_MASK;
    } else if ((remap & DBGAFR_NUMBITS_MASK) == DBGAFR_NUMBITS_MASK) {
        tmp1 = ((uint32_t)0x03) << tmpmask;
        tmpreg &= ~tmp1;
        tmpreg |= ~DBGAFR_SWJCFG_MASK;
    } else {
        tmpreg &= ~(tmp << ((remap >> 0x15) * 0x10));
        tmpreg |= ~DBGAFR_SWJCFG_MASK;
    }

    if (enable)
        tmpreg |= (tmp << ((remap >> 0x15) * 0x10));

    if ((remap & 0x80000000) == 0x80000000)
        AFIO->MAPR2 = tmpreg;
    else
        AFIO->MAPR = tmpreg;
}
