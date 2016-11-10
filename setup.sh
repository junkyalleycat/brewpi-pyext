#!/bin/sh

AVRDIR=$1

if [ -z "$AVRDIR" ]; then
    echo "Please specify base brewpi-avr directory"
    exit 1
fi

mkdir -p links
mkdir -p build

for fn in ActuatorAutoOff.h Actuator.cpp Actuator.h ArduinoEepromAccess.h EepromAccess.h EepromManager.h EepromTypes.h FastDigitalPin.h FilterCascaded.cpp FilterCascaded.h FilterFixed.cpp FilterFixed.h Logger.h LogMessages.h PiLink.h Pins.h RotaryEncoder.h Sensor.cpp Sensor.h TempControl.cpp TempControl.h TemperatureFormats.cpp TemperatureFormats.h TempSensorBasic.h TempSensor.cpp TempSensorDisconnected.h TempSensor.h TempSensorMock.h TicksArduino.h Ticks.cpp Ticks.h; do
    ln -s $AVRDIR/brewpi_avr/$fn links/
done

ln -s $AVRDIR/brewpi_cpp/ArrayEepromAccess.h links/
