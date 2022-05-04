/**
 * @file i2c.h
 * @author Shuran Xu (shxu6388@colorado.edu)
 * @brief This source file contains the implementation of the
 * public APIs related to I2C0 peripheral and temperature accessibility.
 * @version 0.1
 * @date 2022-02-01
 * @reference The calculation of the temperature conversion is performed
 * according to the temperature conversion formula provided at page 22
 * in Si7021-A20.pdf
 * @copyright Copyright (c) 2022
 *
 */

#include "i2c.h"
#include "timers.h"
#include "sl_i2cspm.h"
#include "em_i2c.h"
#include "em_assert.h"

#define INCLUDE_LOG_DEBUG 1
#include "src/log.h"

#define MEASURE_TEMP_No_Hold_Master_Mode             (0xF3)
#define SI7021_ADDR                                  (0x40)

// ISL29125 I2C Address
#define ISL29125_I2C_ADDR                            (0x44)

// ISL29125 Registers
#define ISL29125_DEVICE_ID                           (0x00)
#define ISL29125_CONFIG_1                            (0x01)
#define ISL29125_CONFIG_2                            (0x02)
#define ISL29125_CONFIG_3                            (0x03)
#define ISL29125_THRESHOLD_LL                        (0x04)
#define ISL29125_THRESHOLD_LH                        (0x05)
#define ISL29125_THRESHOLD_HL                        (0x06)
#define ISL29125_THRESHOLD_HH                        (0x07)
#define ISL29125_STATUS                              (0x08)
#define ISL29125_GREEN_L                             (0x09)
#define ISL29125_GREEN_H                             (0x0A)
#define ISL29125_RED_L                               (0x0B)
#define ISL29125_RED_H                               (0x0C)
#define ISL29125_BLUE_L                              (0x0D)
#define ISL29125_BLUE_H                              (0x0E)

// ISL29125 Configuration Settings
#define ISL29125_CFG_DEFAULT                         (0x00)

// ISL29125 I2C Clock Frequency
#define ISL29125_I2C_FREQ                            I2C_FREQ_STANDARD_MAX

#define ISL29125_RESET                               (0x46)

// CONFIG1
// Pick a mode, determines what color[s] the sensor samples, if any
#define CFG1_MODE_POWERDOWN 0x00
#define CFG1_MODE_G 0x01
#define CFG1_MODE_R 0x02
#define CFG1_MODE_B 0x03
#define CFG1_MODE_STANDBY 0x04
#define CFG1_MODE_RGB 0x05
#define CFG1_MODE_RG 0x06
#define CFG1_MODE_GB 0x07

// Light intensity range
// In a dark environment 375Lux is best, otherwise 10KLux is likely the best option
#define CFG1_375LUX 0x00
#define CFG1_10KLUX 0x08

#define RANGE_10K_LUX  (10000U)

// Change this to 12 bit if you want less accuracy, but faster sensor reads
// At default 16 bit, each sensor sample for a given color is about ~100ms
#define CFG1_16BIT 0x00
#define CFG1_12BIT 0x10

// Unless you want the interrupt pin to be an input that triggers sensor sampling, leave this on normal
#define CFG1_ADC_SYNC_NORMAL 0x00
#define CFG1_ADC_SYNC_TO_INT 0x20

// CONFIG2
// Selects upper or lower range of IR filtering
#define CFG2_IR_OFFSET_OFF 0x00
#define CFG2_IR_OFFSET_ON 0x80

// Sets amount of IR filtering, can use these presets or any value between 0x00 and 0x3F
// Consult datasheet for detailed IR filtering calibration
#define CFG2_IR_ADJUST_LOW 0x00
#define CFG2_IR_ADJUST_MID 0x20
#define CFG2_IR_ADJUST_HIGH 0x3F

// CONFIG3
// No interrupts, or interrupts based on a selected color
#define CFG3_NO_INT 0x00
#define CFG3_G_INT 0x01
#define CFG3_R_INT 0x02
#define CFG3_B_INT 0x03


//static I2C_TransferSeq_TypeDef transferSequence;
static uint8_t SI7021_write_data[2];
static uint8_t SI7021_read_data[2];
static uint8_t ISL29125_write_data[4];
static uint8_t ISL29125_read_data[6];
static uint16_t rgb[3];
static uint32_t xyz[3];
I2C_TransferSeq_TypeDef ISL29125_seq;
I2C_TransferSeq_TypeDef I7021_seq;
uint8_t deviceID = 0;
uint8_t isl29125_config_data = 0;


/**
 * @brief This function initializes I2C0.
 */
void initI2C0_for_ISL29125()
{
  I2CSPM_Init_TypeDef I2C0_Config = {
          .port            = I2C0,
          .sclPort         = gpioPortD,
          .sclPin          = 10,
          .sdaPort         = gpioPortD,
          .sdaPin          = 11,
          .portLocationScl = 17,
          .portLocationSda = 19,
          .i2cRefFreq      = 0,
          .i2cMaxFreq      = I2C_FREQ_STANDARD_MAX,
          .i2cClhr         = i2cClockHLRStandard
      };

  I2CSPM_Init(&I2C0_Config);
}

void initI2C0_for_I7021()
{
  I2CSPM_Init_TypeDef i2c_config = {
      .port = I2C0,
      .sclPort = gpioPortC,
      .sclPin = 10,
      .sdaPort = gpioPortC,
      .sdaPin = 11,
      .portLocationScl = 14,
      .portLocationSda = 16,
      .i2cRefFreq = 0,
      .i2cMaxFreq = I2C_FREQ_STANDARD_MAX,
      .i2cClhr = i2cClockHLRStandard
  };

  I2CSPM_Init(&i2c_config);
}


/**
 * Interrupt-based I2C transaction function for ISL29125 using I2C0.
 * @param flag: I2C transaction type
 * @param writeCmd: buffer holding the data to be written
 * @param writeLen: the length of the write data buffer
 * @param readCmd: buffer holding the data to be read
 * @param readLen: the length of the read data buffer
 */
static void ISL29125_transaction_ISR(uint16_t flag,
                                 uint8_t *writeCmd,
                                 size_t writeLen,
                                 uint8_t *readCmd,
                                 size_t readLen)
{

  I2C_TransferReturn_TypeDef ret;

  ISL29125_seq.addr = ISL29125_I2C_ADDR << 1;
  ISL29125_seq.flags = flag;

  switch (flag) {
    // Send the write command from writeCmd, possibly with writeData
    case I2C_FLAG_WRITE:
      ISL29125_seq.buf[0].data = writeCmd;
      ISL29125_seq.buf[0].len  = writeLen;

      break;

    // Receive data into readCmd of readLen
    case I2C_FLAG_READ:
      ISL29125_seq.buf[0].data = readCmd;
      ISL29125_seq.buf[0].len  = readLen;

      break;

    // Send the write command from writeCmd
    // and receive data into readCmd of readLen
    case I2C_FLAG_WRITE_READ:
      ISL29125_seq.buf[0].data = writeCmd;
      ISL29125_seq.buf[0].len  = writeLen;

      ISL29125_seq.buf[1].data = readCmd;
      ISL29125_seq.buf[1].len  = readLen;

      break;

    case I2C_FLAG_WRITE_WRITE:
      ISL29125_seq.buf[0].data = &writeCmd[0];
      ISL29125_seq.buf[0].len  = 1;

      ISL29125_seq.buf[1].data = &writeCmd[1];
      ISL29125_seq.buf[1].len  = writeLen - 1;
      break;

    default:
      break;
  }


  //configure NVIC to generate an IRQ for the I2C0 module
  if(!NVIC_GetEnableIRQ(I2C0_IRQn)){
      NVIC_ClearPendingIRQ(I2C0_IRQn);
      NVIC_EnableIRQ(I2C0_IRQn);
  }

  //initiate the I2C transfer
  ret = I2C_TransferInit(I2C0, &ISL29125_seq);
  if(ret < 0){
      LOG_ERROR("I2C_TransferInit( ) error = %d\r\n", ret);
  }

  return;
}

/**
 * Polling-based I2C transaction function for ISL29125 using I2C0.
 * @param flag: I2C transaction type
 * @param writeCmd: buffer holding the data to be written
 * @param writeLen: the length of the write data buffer
 * @param readCmd: buffer holding the data to be read
 * @param readLen: the length of the read data buffer
 */
static I2C_TransferReturn_TypeDef ISL29125_transaction_POLL(uint16_t flag,
                                 uint8_t *writeCmd,
                                 size_t writeLen,
                                 uint8_t *readCmd,
                                 size_t readLen)
{
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;

  seq.addr = ISL29125_I2C_ADDR << 1;
  seq.flags = flag;

  switch (flag) {
    // Send the write command from writeCmd, possibly with writeData
    case I2C_FLAG_WRITE:
      seq.buf[0].data = writeCmd;
      seq.buf[0].len  = writeLen;

      break;

    // Receive data into readCmd of readLen
    case I2C_FLAG_READ:
      seq.buf[0].data = readCmd;
      seq.buf[0].len  = readLen;

      break;

    // Send the write command from writeCmd
    // and receive data into readCmd of readLen
    case I2C_FLAG_WRITE_READ:
      seq.buf[0].data = writeCmd;
      seq.buf[0].len  = writeLen;

      seq.buf[1].data = readCmd;
      seq.buf[1].len  = readLen;

      break;

    case I2C_FLAG_WRITE_WRITE:
      seq.buf[0].data = &writeCmd[0];
      seq.buf[0].len  = 1;

      seq.buf[1].data = &writeCmd[1];
      seq.buf[1].len  = writeLen - 1;
      break;

    default:
      return i2cTransferUsageFault;
  }

  //initiate the I2C transfer
  ret = I2CSPM_Transfer(I2C0, &seq);

  return ret;
}

/**
 * Reset ISL29125 upon the power-up
 */
void reset_ISL29125()
{
  I2C_TransferReturn_TypeDef ret;
  ISL29125_write_data[0] = ISL29125_DEVICE_ID;
  ISL29125_write_data[1] = ISL29125_RESET;
  ret = ISL29125_transaction_POLL(I2C_FLAG_WRITE_WRITE, &ISL29125_write_data[0], 2, NULL, 0);
  EFM_ASSERT(ret == i2cTransferDone);
  return;
}

/**
 * Read the device ID, this should be prior to the device reset.
 */
void read_deviceID()
{
  ISL29125_write_data[0] = ISL29125_DEVICE_ID;
  ISL29125_transaction_ISR(I2C_FLAG_WRITE_READ, &ISL29125_write_data[0], 1, &deviceID, 1);
}

/**
 * Configure Configuration register 1-3 of the light sensor.
 */
void configure_ISL29125()
{
  I2C_TransferReturn_TypeDef ret;
  ISL29125_write_data[0] = ISL29125_CONFIG_1;
  ISL29125_write_data[1] = CFG1_MODE_RGB | CFG1_10KLUX;
  ISL29125_write_data[2] = CFG2_IR_ADJUST_HIGH;
  ISL29125_write_data[3] = ISL29125_CFG_DEFAULT;
  ret = ISL29125_transaction_POLL(I2C_FLAG_WRITE_WRITE, &ISL29125_write_data[0], 4, NULL, 0);
  EFM_ASSERT(ret == i2cTransferDone);
  return;
}

/**
 * Read the values of configuration registers 1-3 upon configuring
 * the configuration register 1-3.
 */
void read_ISL29125_configuration()
{
  ISL29125_write_data[0] = ISL29125_CONFIG_1;
  ISL29125_transaction_ISR(I2C_FLAG_WRITE_READ, &ISL29125_write_data[0], 1, &isl29125_config_data, 1);
}

/**
 * Measure the sensor data in R,G,B channels.
 */
void ISL29125_measure_RGB()
{
  // Start measurement for GREEN_L DATA and wait for data to become ready
  ISL29125_write_data[0] = ISL29125_GREEN_L;
  ISL29125_transaction_ISR(I2C_FLAG_WRITE_READ, &ISL29125_write_data[0], 1, &ISL29125_read_data[0], 6);
  return;
}

/**
 * RGB -> XYZ transformation.
 */
void ISL29125_transform_RBG_to_XYZ()
{
  float Cxr = 0.6327, Cxg = 0.2046, Cxb = 0.1269;
  float Cyr = 0.2285, Cyg = 0.7374, Cyb = 0.0342;
  float Czr = 0, Czg = 0.0095, Czb = 0.8157;

  // Update rgb with ISL29125_read_data
  // rgb[0] represents green
  rgb[0] = ISL29125_read_data[0];
  rgb[0] |= (ISL29125_read_data[1] << 8);
  // rgb[1] represents red
  rgb[1] = ISL29125_read_data[2];
  rgb[1] |= (ISL29125_read_data[3] << 8);
  // rgb[2] represents blue
  rgb[2] = ISL29125_read_data[4];
  rgb[2] |= (ISL29125_read_data[5] << 8);

//  LOG_INFO("R = %d B = %d G = %d\r\n", rgb[1], rgb[2], rgb[0]);

  // X = Cxr * R + Cxg * G + Cxb * B
  xyz[0] = (uint32_t)(Cxr * rgb[1] + Cxg * rgb[0] + Cxb * rgb[2]);
  // Y = Cyr * R + Cyg * G + Cyb * B
  xyz[1] = (uint32_t)(Cyr * rgb[1] + Cyg * rgb[0] + Cyb * rgb[2]);
  // Z = Czr * R + Czg * G + Czb * B
  xyz[2] = (uint32_t)(Czr * rgb[1] + Czg * rgb[0] + Czb * rgb[2]);

//  LOG_INFO("X = %u Y = %u Z = %u\r\n", xyz[0], xyz[1], xyz[2]);

  return;
}

/**
 * Calculate the light intensity in units of lux based on
 * the configured lux range
 * @return uint32_t: the light density value in lux
 */
uint32_t calculate_light_density_in_lux()
{
  return xyz[1];
}


/**
 * Interrupt-based I2C transaction function for I7021 using I2C1.
 * @param flag: I2C transaction type
 * @param writeCmd: buffer holding the data to be written
 * @param writeLen: the length of the write data buffer
 * @param readCmd: buffer holding the data to be read
 * @param readLen: the length of the read data buffer
 */
static void I7021_transaction_ISR(uint16_t flag,
                                 uint8_t *writeCmd,
                                 size_t writeLen,
                                 uint8_t *readCmd,
                                 size_t readLen)
{
  I2C_TransferReturn_TypeDef ret;

  I7021_seq.addr = SI7021_ADDR << 1;
  I7021_seq.flags = flag;

  switch (flag) {
      // Send the write command from writeCmd, possibly with writeData
      case I2C_FLAG_WRITE:
        I7021_seq.buf[0].data = writeCmd;
        I7021_seq.buf[0].len  = writeLen;

        break;

      // Receive data into readCmd of readLen
      case I2C_FLAG_READ:
        I7021_seq.buf[0].data = readCmd;
        I7021_seq.buf[0].len  = readLen;

        break;

      // Send the write command from writeCmd
      // and receive data into readCmd of readLen
      case I2C_FLAG_WRITE_READ:
        I7021_seq.buf[0].data = writeCmd;
        I7021_seq.buf[0].len  = writeLen;

        I7021_seq.buf[1].data = readCmd;
        I7021_seq.buf[1].len  = readLen;

        break;

      case I2C_FLAG_WRITE_WRITE:
        I7021_seq.buf[0].data = &writeCmd[0];
        I7021_seq.buf[0].len  = 1;

        I7021_seq.buf[1].data = &writeCmd[1];
        I7021_seq.buf[1].len  = writeLen - 1;
        break;

      default:
        break;
  }

  if(!NVIC_GetEnableIRQ(I2C0_IRQn)){
      NVIC_ClearPendingIRQ(I2C0_IRQn);
      NVIC_EnableIRQ(I2C0_IRQn);
  }

  //initiate the I2C transfer
  ret = I2C_TransferInit(I2C0, &I7021_seq);
  if(ret < 0){
      LOG_ERROR("I2C_TransferInit( ) error = %d\r\n", ret);
  }

  return;
}

/**
 * @brief Send 0xF3 command to the sensor via I2C1 using
 * interrupt-driven I2C_TransferInit( ).
 */
void I7021_write()
{
  SI7021_write_data[0] = MEASURE_TEMP_No_Hold_Master_Mode;
  I7021_transaction_ISR(I2C_FLAG_WRITE, &SI7021_write_data[0], 1, NULL, 0);
}

/**
 * @brief Receive the sensor response based on the
 * command 0xF3 sent to the sensor via I2C1. The operation
 * is performed via interrupt-driven I2C_TransferInit( ).
 */

void I7021_read()
{
  I7021_transaction_ISR(I2C_FLAG_READ, NULL, 0, &SI7021_read_data[0], 2);
}


/**
 * @brief obtain the temperature data read from the Si7021 sensor.
 */
uint32_t get_temperature_data()
{
  uint16_t tempData = SI7021_read_data[0] << 8;
  tempData |= SI7021_read_data[1];
  //calculates the temperature value in Celsius according to the
  //temperature conversion of the Si7021 sensor.
  float temperature = (175.72 * tempData) / 65536 - 46.85;
//  LOG_INFO("Temperature %.2f Celsius\r\n",temperature);
  return (uint32_t)temperature;
}

