# BLE-Smart-Bedroom-Environment-Monitor

![alt text](./images/iot-fg.jpg)

## Problems Statement

Sleep trouble has been one of the ongoing threats to individual health worldwide. According to the National Institutes of Health (NIH) , approximately 20% to 30% of adults in the U.S. alone have experienced insomnia symptoms, with at least 6% suffering from insomnia disorder. [1] The research has identified that the following key factors impact the sleep quality considerably:[2]

Bedroom Temperature: low temperature on average makes people feel sleepy, whereas a higher temperature keeps individuals awake.
Noise:  Statistics have shown that loud noise disturbances can cause severe sleep fragmentation and disruption, which in turn can have negative impacts on individuals’ physical and mental health.
Light: Studies have found that exposure to strong light sources later in the day can lead to more nocturnal awakenings and less slow-wave sleep, which is  a portion of a sleep cycle that is vital to cell repair and bodily restoration.  It is a good practice to keep the bedroom light levels as low as possible to promote sleep. 

Hence, it is crucial to adjust the above factors properly to create an optimal sleep environment to improve the sleep quality. 

References: <br/>
[1]https://www.goodpath.com/learn/statistics-on-insomnia-in-us-goodpath-results <br/>
[2]https://www.sleepfoundation.org/bedroom-environment


## Project Overview

This project is a Low-Power-Bluetooth-based smart bedroom environment monitor which will promote good sleep quality by measuring and adjusting temperature, light and noise instantly for optimal sleep quality.  According to the research, the team found the following conditions that are promoting better sleep:

- A temperature between 15.6C - 22.0C (changes from person to person)
- No noise disturbance (sound should be between 40db- 55 db)
- During bedtime the lux reading should be less than 180 (inside the house and less than 5 after lights are turned off (inside the bedroom).

The product will monitor and adjust the above three factors of the bedroom environment using 3 sensors so as to meet the above optimal conditions. In addition, the product enables the user to sleep better at any time by promoting the user to set the sleep time and configuring temperature, light and noise values correspondingly and automatically. 

## Hardware Block Diagram

![alt text](./images/hw-block-diagram.png)

The hardware block diagram shown above is the gist of hardware we will be using in the project. The main two parts are GATT server and GATT Client. The server will have all the sensors connected to it. The sensors that we are using are:

- On-board temperature sensor
- Light sensor
- Sound sensor

The interfaces used for each sensor is also shown in the diagram. Temperature and light sensor work on the I2C bus and the sound sensor will work using the ADC. The LEDs and the LCD display are on board and will be used accordingly. The push button will be used by the user to set sleep timing and will have a GPIO interface. 

## Software Block Diagram

![alt text](./images/sw-block1.png)

![alt text](./images/sw-block2.png)


The GATT server requires drivers to manipulate all the attached sensors and silab APIs to manipulate the low-power timer.  And the GATT client needs a custom-built library to convert the target sleep time to associated temperature, light and noise level values. In addition to the above, both parties need APIs to access the LCD display and the GPIO library to manipulate push buttons as well as LEDs. Furthermore, there will be software implementations for the state machines on both the client device and the server device to achieve interaction between each other and present the user interface on both devices. 

## Software Flow Diagram

![alt text](./images/sw-flow.png)


## Energy Consumption

As the system is composed of both the client device and the server device, the energy consumption performance analysis for each device will be provided in this section. Specifically, the analysis for different states on both devices will be carried out for the comprehensive energy performance analysis. And the team used the Silicon Lab’s Energy Profiler to characterize the low power performance of the GATT server and the GATT client.


### Client Device



