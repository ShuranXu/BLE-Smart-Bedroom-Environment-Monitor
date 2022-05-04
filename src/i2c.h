/**
 * @file i2c.h
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This header file contains public APIs related to I2C0 peripheral and
 * temperature accessibility.
 * @version 0.1
 * @date 2022-02-01
 *
 * @copyright Copyright (c) 2022
 *
 */

#ifndef __I2C_H__
#define __I2C_H__

#include <stdint.h>

/**
 * @brief This function initializes I2C0.
 */

void initI2C0_for_I7021();
void initI2C0_for_ISL29125();

/**
 * @brief This function initializes I2C1.
 */
void initI2C1();

/**
 * Reset ISL29125 upon the power-up
 */
void reset_ISL29125();

/**
 * Configure Configuration register 1-3 of the light sensor.
 */
void configure_ISL29125();

/**
 * Measure the sensor data in R,G,B channels.
 */
void ISL29125_measure_RGB();

/**
 * RGB -> XYZ transformation.
 */
void ISL29125_transform_RBG_to_XYZ();

/**
 * Calculate the light intensity in units of lux based on
 * the configured lux range
 * @return uint32_t: the light density value in lux
 */
uint32_t calculate_light_density_in_lux();

/**
 * Read the device ID, this should be prior to the device reset.
 */
void read_deviceID();

/**
 * Read the values of configuration registers 1-3 upon configuring
 * the configuration register 1-3.
 */
void read_ISL29125_configuration();

/**
 * @brief Send 0xF3 command to the sensor via I2C1 using
 * interrupt-driven I2C_TransferInit( ).
 */
void I7021_write();
/**
 * @brief Receive the sensor response based on the
 * command 0xF3 sent to the sensor via I2C1. The operation
 * is performed via interrupt-driven I2C_TransferInit( ).
 */
void I7021_read();
/**
 * @brief obtain the temperature data read from the Si7021 sensor.
 */
uint32_t get_temperature_data();


#endif //__I2C_H__
