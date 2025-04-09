# EFR-SGM-C8-smart-interface
This project reads the SML-interface of the EFR-SGM-C8 energy meter via an ESP32 and then provides a local web server to show the extracted information.  
This is useful if your meter does not have the "smart" module installed, which net providers like to omit, because... money.  
You can then use the web server to conveniently check you energy usage and adjust the output of your solar storage to not waste any energy or give it away, helping you save even more money.  
Additionally, the API that the website uses to keep itself updated is also available for you to use in other projects.  

**Planned:** In the near future I would also like to add some logging capabilities to the project via an SD card module. This would allow us to display historic data on the website (last 24h/7d/30d/...) which the meter itself collects but does not provide via the IR-interface. Also the data can be used to generate some graphs.  

## Hardware
The project is designed around an ESP32 Mini development board and an TCRT5000 IR sensor.  
Additionally magnets are used to mount the project on the energy-meter.  
The links below are there to show visually you what I mean, not buying recommendations.  

Product | Link
-|-
ESP32 Mini dev board | https://www.az-delivery.de/en/products/esp32-d1-mini-nodemcu-wifi-modul-esp-32-bluetooth-internet-entwicklungsboard-mit-usb-c-anschluss-kompatibel-mit-arduino-und-kompatibel-mit-wemos-d1-mini?_pos=3&_sid=d7de684df&_ss=r
TCRT5000 IR sensor | https://www.az-delivery.de/en/products/linienfolger-modul-mit-tcrt5000-und-analog-ausgang?_pos=1&_sid=747c4ebcb&_ss=r
Round magnets with ~20 mm diameter | no link needed, very generic

Yes, it would also be possible to entirely skip the IR sensor and use only a photo-resistor.  
But I like that the sensor board comes with a potentiometer to easily adjust the sensitivity (which is very important!!!) and that it has an easily accessible mounting point.  

## Installation
### Software
Flashing the ESP with the program should be pretty straightforward:  
1. Clone the repository to your computer
2. Edit the files in [YOUR_CONFIGURATION](./YOUR_CONFIGURATION) to reflect your WiFi setup and your time zone. To select the correct time zone you can refer to the column on the right of [this](https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv) list. Make sure that these files **only** contain the information that their name indicates in **one** line with **no** additional spaces or other characters before and after your entry.
3. Build and flash the project with ESP-IDF.

### Hardware
The hardware setup is also very simple.  
You only need to supply the IR sensor with 3.3V and GND and you need to connect pin 18 of the ESP with the digital out pin of the sensor.  
Now you only need to assemble the 3D-print and mount it on your energy-meter (see below).  

## CAD/3D-models
The current version of the mount is a pretty rough prototype at the moment, some dimensions are a tiny bit off. It can be used, but is not ideal.  
I will provide a more polished version ASAP.  