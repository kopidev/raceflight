/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "platform.h"
#include "debug.h"

#include "common/axis.h"
#include "common/maths.h"

#include "system.h"
#include "exti.h"
#include "gpio.h"
#include "bus_spi.h"

#include "sensor.h"
#include "accgyro.h"
#include "accgyro_mpu.h"
#include "accgyro_mpu6500.h"
#include "accgyro_spi_mpu6500.h"

#define DISABLE_MPU6500       GPIO_SetBits(MPU6500_CS_GPIO,   MPU6500_CS_PIN)
#define ENABLE_MPU6500        GPIO_ResetBits(MPU6500_CS_GPIO, MPU6500_CS_PIN)

extern uint16_t acc_1G;

bool mpu6500WriteRegister(uint8_t reg, uint8_t data)
{
    ENABLE_MPU6500;
    delayMicroseconds(1);
    spiTransferByte(MPU6500_SPI_INSTANCE, reg);
    spiTransferByte(MPU6500_SPI_INSTANCE, data);
    DISABLE_MPU6500;
    delayMicroseconds(1);

    return true;
}

bool verifympu6500WriteRegister(uint8_t reg, uint8_t data) {

    uint8_t in;
    uint8_t attemptsRemaining = 20;
    static uint32_t loop = 0;
    mpu6500WriteRegister(reg, data);
    delayMicroseconds(100);

    do {
        mpu6500SlowReadRegister(reg, 1, &in);
        if (in == data) {
            return true;
        } else {
            mpu6500WriteRegister(reg, data);
            delayMicroseconds(100);
        }
    } while (attemptsRemaining--);
    return true; //returning false here causes a usage fault
}

bool mpu6500ReadRegister(uint8_t reg, uint8_t length, uint8_t *data)
{
    ENABLE_MPU6500;
    spiTransferByte(MPU6500_SPI_INSTANCE, reg | 0x80); // read transaction
    spiTransfer(MPU6500_SPI_INSTANCE, data, NULL, length);
    DISABLE_MPU6500;

    return true;
}

bool mpu6500SlowReadRegister(uint8_t reg, uint8_t length, uint8_t *data)
{
    ENABLE_MPU6500;
    delayMicroseconds(1);
    spiTransferByte(MPU6500_SPI_INSTANCE, reg | 0x80); // read transaction
    spiTransfer(MPU6500_SPI_INSTANCE, data, NULL, length);
    delayMicroseconds(1);
    DISABLE_MPU6500;

    return true;
}

static void mpu6500SpiInit(void)
{
    static bool hardwareInitialised = false;

    if (hardwareInitialised) {
        return;
    }

#ifdef STM32F303xC
    RCC_AHBPeriphClockCmd(MPU6500_CS_GPIO_CLK_PERIPHERAL, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = MPU6500_CS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(MPU6500_CS_GPIO, &GPIO_InitStructure);
#endif

#if defined(STM32F40_41xxx) || defined (STM32F411xE)
    RCC_APB2PeriphClockCmd(MPU6500_CS_GPIO_CLK_PERIPHERAL, ENABLE);

    gpio_config_t gpio;
    // CS as output
    gpio.mode = Mode_Out_PP;
    gpio.pin = MPU6500_CS_PIN;
    gpio.speed = GPIO_Speed_100MHz;
    gpioInit(MPU6500_CS_GPIO, &gpio);
#endif

    GPIO_SetBits(MPU6500_CS_GPIO,   MPU6500_CS_PIN);

#if defined(STM32F40_41xxx) || defined (STM32F411xE)
    spiSetDivisor(MPU6500_SPI_INSTANCE, SPI_SLOW_CLOCK);
#else
    spiSetDivisor(MPU6500_SPI_INSTANCE, SPI_STANDARD_CLOCK);
#endif

    hardwareInitialised = true;
}

bool mpu6500SpiDetect(void)
{
    uint8_t tmp;

    mpu6500SpiInit();

    mpu6500ReadRegister(MPU_RA_WHO_AM_I, 1, &tmp);

    if (tmp == MPU6500_WHO_AM_I_CONST || tmp == MPU9250_WHO_AM_I_CONST) {
        return true;
    }

    return false;
}

bool mpu6500SpiAccDetect(acc_t *acc)
{
    if (mpuDetectionResult.sensor != MPU_65xx_SPI) {
        return false;
    }

    acc->init = mpu6500AccInit;
    acc->read = mpuAccRead;

    return true;
}

bool mpu6500SpiGyroDetect(gyro_t *gyro)
{
    if (mpuDetectionResult.sensor != MPU_65xx_SPI) {
        return false;
    }

    gyro->init = mpu6500GyroInit;
    gyro->read = mpuGyroRead;
    gyro->intStatus = checkMPUDataReady;

    // 16.4 dps/lsb scalefactor
    gyro->scale = 1.0f / 16.4f;

    return true;
}
