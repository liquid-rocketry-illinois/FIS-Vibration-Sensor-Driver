# MEMs Accelerometer Based Vibration Sensor
## Introduction

[Documentation](https://docs.google.com/document/d/1U1gItN9JbBFW0opQRtAksD6sWcRA72sNK8zBZmeB6xE/edit?tab=t.lwzczn30unl4) <br/>
The current implementation of the accelerometer driver is built to support I2C/SPI communication with the ADXL345 (Adafruit) board. The ADXL345 converts tri-axial acceleration (X, Y, Z axes; labelled on the board) into 16-bit sign-extended 2's complement 10-bit values. <br/>

## Usage

[placeholder] <br/>

## Main Parts
**ADXL345**: [Adafruit ADXL345 Breakout Board](https://www.adafruit.com/product/1231), [ADXL345 IC](https://www.analog.com/en/products/adxl345.html) <br/>
**STM32F303RET6**: [Product Website](https://www.st.com/en/evaluation-tools/nucleo-f303re.html), [Schematic](https://www.st.com/resource/en/schematic_pack/mb1136-default-c03_schematic.pdf), [HAL](https://www.st.com/resource/en/user_manual/um1786-description-of-stm32f3-hal-and-lowlayer-drivers-stmicroelectronics.pdf), [Reference Manual](https://www.st.com/resource/en/reference_manual/DM00043574.pdf) <br/>

## Specifications

[placeholder] <br/>

## Compiling

[placeholder] <br/>

## Reference

The driver is written to interface with the STM32F303RET6. With respect to the .ioc file, all references are to the STM32F3 HAL and configured according to the STM32F3 schematic. The .ioc file currently enables the SPI2 peripheral: PF1 > SCLK, PB15 > MOSI, PB14 > MISO
