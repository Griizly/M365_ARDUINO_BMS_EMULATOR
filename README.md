# M365_ARDUINO_BMS_EMULATOR
This project intend to create an open source BMS emulator for the Xiaomi M365


Already tested the code with an arduino nano
The result is that the scooter turn on without beeping
In the m365 tools android app we see the fake values sent by the ARDUINO NANO to the scooter


What doesn't work : riding ...
Couldn't find what I have to trigger to enable riding.


I don't have time to continue this project so I share it


My work is based on the work of BotoX https://github.com/BotoX/xiaomi-m365-compatible-bms

ARDUINO NANO        M365 mother board
RX            ->    T

TX            ->    R

VIN           ->    5

GND           ->    G


M365 motherboard
![alt text](https://i.imgur.com/lZtx8rl.jpg)


Arduino NANO
![alt text](http://2.bp.blogspot.com/-nGbwbpqz7xU/UJW2yoRcd7I/AAAAAAAATdI/KG95uVsLgdM/s1600/Arduino+nano+Pinout.png)
