# Shinwa Battery Management System

16S Lithium iron phosphate Battery Management System (LiFePO4 BMS) - that's all I knew about this board at the very beginning. I couldn’t find any software on the manufacturer’s website, no one answered by e-mail either, but I managed to google someone’s [reverse engineering of the protocol](shinwa-bms-protocol-v11.pdf). This was very helpful because BMS was not making contact and was waiting for a wake-up message.

# Hardware

ESP8266 module (esp12e board). RS485 driver bypassed by directly connecting to [optocoupler inputs](gpio-connection.jpg).

# Software

Esphome based node with custom module [shinwabms.h](shinwabms.h) and pretty simple config [shinwabms.yaml](shinwabms.yaml).

## Implemented features
- Battery voltage
- Per cell voltage 
- Load current
- Temperature sensors (°C)
- Battery capacity (Ah)
- State of charge
- State of health
- Cycle count

## Unimplemented features
- Alarm flags
- Configuration changes