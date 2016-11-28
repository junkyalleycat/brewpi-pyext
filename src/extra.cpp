/**
  The python glue code is entirely contained in this file.  The code
  is pretty streightforward, define a new python type (TempControl) and
  provide it with all relavent methods of TempControl.cpp.  The methods
  in this class are passthroughs and do very little except some unit
  translation.

  There are a few methods that need to be defined but aren't used, these
  are included in a block towards the top of this file.
  */

#include "TempControl.h"
#include "TempSensorDisconnected.h"
#include "PiLink.h"
#include <stdio.h>
#include <Python.h>
#include <stdexcept>
#include <sys/time.h>
#include <typeinfo>
#include "utils.h"
#include "cpy.h"
#include <memory>

// defaults taken from DeviceManager.cpp
ValueSensor<bool> defaultSensor(false);  
ValueActuator defaultActuator;           
DisconnectedTempSensor defaultTempSensor;
// loaded normally in Brewpi.cpp
TicksImpl ticks = TicksImpl();

// called when TempControl door is open/shut, i don't have a door switch
void PiLink::printFridgeAnnotation(char const *, ...) {
    PyErr_SetString(PyExc_RuntimeError, "unimplemented: printFridgeAnnotation"); 
    throw std::exception();
}

// not called
void delay(unsigned long v) {
    PyErr_SetString(PyExc_RuntimeError, "unimplemented: delay"); 
    throw std::exception();
}

// called only when TempControl.load is called
void eeprom_read_block(void *__dst, const void *__src, size_t __n) {
    PyErr_SetString(PyExc_RuntimeError, "unimplemented: eeprom_read_block"); 
    throw std::exception();
}

// called only when TempControl.save is called
void eeprom_update_block(const void *__src, void *__dst, size_t __n) {
    PyErr_SetString(PyExc_RuntimeError, "unimplemented: eeprom_update_block"); 
    throw std::exception();
}

/*
   The following methods are actually called and need impl
   */
// called from TempControl, like once
void Logger::logMessageVaArg(char type, LOG_ID_TYPE errorID, const char * varTypes, ...) {
    // logger too weird to implement, don't care about log messages right now
}

// used by Ticks
unsigned long millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long long millisecondsSinceEpoch =
        (unsigned long long)(tv.tv_sec) * 1000 +
        (unsigned long long)(tv.tv_usec) / 1000;
    return millisecondsSinceEpoch;
}

// called from TempControl whenever temp changes
void EepromManager::storeTempSettings() {
    // do nothing, temp is provided by construction
    // if one wishes to serialize the settings, use
    // store and load
}
