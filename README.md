# tstat

Thermostat management service.

## Target

This code is made to run on a Raspberry Pi Zero. The installations where it is used run Arch Linux ARM. A systemd service file is included.

There is no UI included; at present, temperature changes are made by (f)cron invoking `settempf <low> <high>` on an appropriate schedule.

## Devices

The Pi is powered by the 24V thermostat power, taken to 5V through an adjustable buck converter.

Thermostat pins (which go to an NC relay) are described in the pin constants in tstat.cc.

The sensor used is a Maxim DS18B20 1-wire digital thermometer using the w1_therm kernel driver.

## Dependencies

[Poco](https://pocoproject.org/) and [pigpio](https://abyz.me.uk/rpi/pigpio/).

## Author

David B. Robins

Licensed under the GPL v3.0.
