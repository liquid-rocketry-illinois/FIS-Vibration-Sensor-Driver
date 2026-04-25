# MEMs Accelerometer Based Vibration Sensor
## Introduction

[Documentation](https://docs.google.com/document/d/1U1gItN9JbBFW0opQRtAksD6sWcRA72sNK8zBZmeB6xE/edit?tab=t.lwzczn30unl4) <br/>
The current implementation of the accelerometer driver is built to support I2C/SPI communication with the ADXL345 (Adafruit) board. The ADXL345 converts tri-axial acceleration (X, Y, Z axes; labelled on the board) into 16-bit sign-extended 2's complement 10-bit values. <br/>

## Usage

<img width="480" height="360" alt="thermocouple_driver_wiring" src="https://github.com/liquid-rocketry-illinois/FIS-Vibration-Sensor-Driver/blob/dev/imagery/vbs.jpg"/> <br/>
The test connections to be made between the STM32H753ZI Nucleo-144 and MCP960 are to be made as follows: <br/>
<table border="1" style="border-collapse: collapse;">
   <thead>
       <tr>
           <th>ADXL345       </th>
           <th>STM32F303RET6 </th>
           <th>WIRE COLOR    </th>
       </tr>
   </thead>
   <tbody>
       <tr>
           <td>VIN      </td>
           <td>3.3V     </td>
           <td>RED      </td>
       </tr>
       <tr>
           <td>GND      </td>
           <td>GND      </td>
           <td>BLACK    </td>
       </tr>
       <tr>
           <td>SCL (SCK)  </td>
           <td>PF1        </td>
           <td>YELLOW     </td>
       </tr>
       <tr>
           <td>SDA (MOSI) </td>
           <td>PB15       </td>
           <td>BLUE       </td>
       </tr>
       <tr>
           <td>SDO (MISO) </td>
           <td>PB14       </td>
           <td>ORANGE     </td>
       </tr>
       <tr>
           <td>CS         </td>
           <td>A0         </td>
           <td>PURPLE     </td>
       </tr>
   </tbody>
</table>

## Main Parts
**ADXL345**: [Adafruit ADXL345 Breakout Board](https://www.adafruit.com/product/1231), [ADXL345 IC](https://www.analog.com/en/products/adxl345.html) <br/>
**STM32F303RET6**: [Product Website](https://www.st.com/en/evaluation-tools/nucleo-f303re.html), [Schematic](https://www.st.com/resource/en/schematic_pack/mb1136-default-c03_schematic.pdf), [HAL](https://www.st.com/resource/en/user_manual/um1786-description-of-stm32f3-hal-and-lowlayer-drivers-stmicroelectronics.pdf), [Reference Manual](https://www.st.com/resource/en/reference_manual/DM00043574.pdf) <br/>

## Specifications

[placeholder] <br/>

## Compiling

[placeholder] <br/>

## Reference

The .ioc enables SPI2, with connections as follows: SPI2_SCK > PF1, SPI_MOSI > PB15, SPI_MISO > PB14. The .ioc also enables 1 GPIO pin, A0, for CS.

The driver is written to interface with the STM32F303RET6. With respect to the .ioc file, all references are to the STM32F3 HAL and configured according to the STM32F3 schematic. The .ioc file currently enables the SPI2 peripheral: PF1 > SCLK, PB15 > MOSI, PB14 > MISO
