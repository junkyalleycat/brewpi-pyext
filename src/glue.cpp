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

// defaults taken from DeviceManager.cpp
ValueSensor<bool> defaultSensor(false);  
ValueActuator defaultActuator;           
DisconnectedTempSensor defaultTempSensor;
// loaded normally in Brewpi.cpp
TicksImpl ticks = TicksImpl();

/*
   It seems that the TempControl code has mostly
   been excercised using celsius, so
   i won't use the format variable in
   TempControl, but will use my own that
   modifies temp at the gate
   */
double c2f(double c) {
    return (c * 1.8) + 32;
}

double f2c(double f) {
    return (f - 32) / 1.8;
}

double tempToDouble(temperature val) {
    return double(val - C_OFFSET) / double(TEMP_FIXED_POINT_SCALE);
}

double tempDiffToDouble(temperature val) {
    return double(val) / double(TEMP_FIXED_POINT_SCALE);
}

/*
   Because a number from python might be a float or an int
   */
double pyNumToDouble(PyObject *pyNum) {
    double val;
    if(PyFloat_Check(pyNum)) {
        val = PyFloat_AsDouble(pyNum);
    } else if(PyLong_Check(pyNum)) {
        val = PyLong_AsLong(pyNum);
    } else {
        PyErr_SetString(PyExc_RuntimeError, "must specify float or long");
        throw std::exception();
    }
    return val;
}

// This class makes handling local python objects easier
// at it moves their decref to the destructor
typedef struct {} NewRef_PyObject;

class CPyObject {

    public:
        virtual operator PyObject *() = 0;
        virtual operator NewRef_PyObject *() = 0;

};

class PPyObject: public CPyObject {
    private:
        PyObject *o;
    public:
        PPyObject(PyObject *o) {
            if(o == NULL) {
                throw std::exception();
            }
            this->o = o;
        }
        ~PPyObject() {
            Py_XDECREF(o);
        }
        operator PyObject *() {
            return o;
        }
        operator NewRef_PyObject *() {
            Py_XINCREF(o);
            return (NewRef_PyObject *) o;
        }
};

class UPyObject: public CPyObject {
    private:
        PyObject *o;
    public:
        UPyObject(PyObject *o) {
            if(o == NULL) {
                throw std::exception();
            }
            Py_XINCREF(o);
            this->o = o;
        }
        ~UPyObject() {
            Py_XDECREF(o);
        }
        operator PyObject *() {
            return o;
        }
        operator NewRef_PyObject *() {
            Py_XINCREF(o);
            return (NewRef_PyObject *) o;
        }
};

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

/*
   shortens a double, i was getting strange results
   before i did this, don't care to investigate
   */
static double shorten(double v) {
    double tmp = int(v * 100);
    tmp /= 100;
    return tmp;
}

/*
   PyBasicTempSensor wraps a BasicTempSensor.  It calls
   into python to find temperature data.

   python interface

   class Sensor:

       def read(unit=[c|f])
   
   */
class PyBasicTempSensor : public BasicTempSensor {

    private:
        CPyObject *py_sensor;

    public:
        PyBasicTempSensor(CPyObject *py_sensor) {
            this->py_sensor = py_sensor;
        }

        ~PyBasicTempSensor() {
            delete(py_sensor);
        } 

        bool isConnected(void) {
            return true;
        }

        bool init(void) {
        }

        temperature read() {
            PPyObject m(PyObject_GetAttrString(*this->py_sensor, "read"));
            PPyObject kw(PyDict_New());
            PPyObject unit(PyUnicode_FromString("c"));
            PyDict_SetItemString(kw, "unit", unit);
            PPyObject args(PyTuple_New(0));
            PPyObject r(PyObject_Call(m, args, kw));
            
            temperature temp;
            if(r == Py_None) {
                temp = TEMP_SENSOR_DISCONNECTED;
            } else {
                double c = pyNumToDouble(r);
                temp = doubleToTemp(shorten(c));
            }

            return temp;
        }

};

/*
   PyActuator is a wrapper around actuator.  It calls into
   python to set a switch on/off
   The pthon class needs an on and off method

   python interface

   class Switch:

       def on()

       def off()
   
   */
class PyActuator : public Actuator {

    private:
        CPyObject *py_switch;

    public:
        PyActuator(CPyObject *py_switch) {
            this->py_switch = py_switch;
        }

        ~PyActuator() {
            delete(this->py_switch);
        }

        void setActive(bool active) {
            PyObject *m_;
            if(active) {
                m_ = PyObject_GetAttrString(*this->py_switch, "on");
            } else {
                m_ = PyObject_GetAttrString(*this->py_switch, "off");
            }
            PPyObject m(m_);
            PPyObject r(PyObject_CallObject(m, NULL)); 
        }

        bool isActive() {
            PyErr_SetString(PyExc_RuntimeError, "unimplemented: isActive"); 
            throw std::exception();
        }

};

// because TempControl is a static class,
// we only want a single "instance" of it
// active in python at a time

static bool initialized = false;

/*
   The object stores any items that were
   set on tempControl that need to be freed
   later.
   */
typedef struct {
    PyObject_HEAD
    TempSensor *beerSensor;
    TempSensor *fridgeSensor;
    PyActuator *heater;
    PyActuator *cooler;
    char unit; // unit used when returning information using getters
} TempControl_Object;

/*
   Anytime a temp is provided as an argument or return
   from python, change it to internal unit
   */
double convertTemp(TempControl_Object *self, double temp_c) {
    if(self->unit == 'c') {
        return temp_c;
    }
    return c2f(temp_c);
}

double convertTempDiff(TempControl_Object *self, double temp_c) {
    if(self->unit == 'c') {
        return temp_c;
    }
    return c2f(temp_c) - 32;
}

static void
TempControl_dealloc__(TempControl_Object *self) {
    if(self->beerSensor != NULL) {
        delete(&self->beerSensor->sensor());
        delete(self->beerSensor);
    }
    if(self->fridgeSensor != NULL) {
        delete(&self->fridgeSensor->sensor());
        delete(self->fridgeSensor);
    }
    if(self->heater != NULL) {
        delete(self->heater);
    }
    if(self->cooler != NULL) {
        delete(self->cooler);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);

    initialized = false;
}

/*
   new method for python.  This exists as well as init because
   i only want a single instance of TempControl in existence,
   so i only allow new to succeed if initialized is false.
   */
static PyObject *
TempControl_new__(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    if(initialized) {
        PyErr_SetString(PyExc_RuntimeError, "tempControl already initalized");
        return NULL;
    }

    TempControl_Object *self;
    self = (TempControl_Object *) type->tp_alloc(type, 0);
    if(self == NULL) {
        return NULL;
    }

    initialized = true;

    return (PyObject *) self;
}

static int TempControl_init__(TempControl_Object *self, PyObject *args, PyObject *kwds) {
    char *unit_str = NULL;
    static const char *kwlist[] = {"unit", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|$s", (char **) kwlist, &unit_str)) {
        return -1;
    }

    // unit should be a string, either 'f' or 'c'
    char unit;
    if(unit_str == NULL) {
        unit = 'c';
    } else if(strcmp(unit_str, "c") == 0) {
        unit = 'c';
    } else if(strcmp(unit_str, "f") == 0) {
        unit = 'f';
    } else {
        PyErr_SetString(PyExc_RuntimeError, "unknown unit specified");
        return -1;
    }

    self->unit = unit;

    self->beerSensor = NULL;
    self->fridgeSensor = NULL;
    self->heater = NULL;
    self->cooler = NULL;

    return 0;
}

/*
   Different from python __init__, this method performs some
   basic initialization of the tempControl object
   */
static PyObject *
TempControl_init(PyObject *self, PyObject *args) {
    try {
        tempControl.init();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_setBeerSensor(TempControl_Object *self, PyObject *args) {
    if(self->beerSensor != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "beerSensor already set");
        return NULL;
    }

    PyObject *py_sensor_;
    if(!PyArg_ParseTuple(args, "O", &py_sensor_)) {
        return NULL;
    }
    UPyObject *py_sensor = new UPyObject(py_sensor_);
    TempSensor *sensor = new TempSensor(TEMP_SENSOR_TYPE_BEER, new PyBasicTempSensor(py_sensor));
    try {
        sensor->init();
    } catch(...) {
        delete(&sensor->sensor());
        delete(sensor);
        return NULL;
    }
    self->beerSensor = sensor;
    tempControl.beerSensor = sensor;

    Py_RETURN_NONE;
}

static PyObject *
TempControl_setFridgeSensor(TempControl_Object *self, PyObject *args) {
    if(self->fridgeSensor != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "fridgeSensor already set");
        return NULL;
    }

    PyObject *py_sensor_;
    if(!PyArg_ParseTuple(args, "O", &py_sensor_)) {
        return NULL;
    }
    UPyObject *py_sensor = new UPyObject(py_sensor_);
    TempSensor *sensor = new TempSensor(TEMP_SENSOR_TYPE_FRIDGE, new PyBasicTempSensor(py_sensor));
    try {
        sensor->init();
    } catch(...) {
        delete(&sensor->sensor());
        delete(sensor);
        return NULL;
    }
    self->fridgeSensor = sensor;
    tempControl.fridgeSensor = sensor;
    Py_RETURN_NONE;
}

static PyObject *
TempControl_setHeater(TempControl_Object *self, PyObject *args) {
    if(self->heater != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "heater already set");
        return NULL;
    }

    PyObject *py_switch_;
    if(!PyArg_ParseTuple(args, "O", &py_switch_)) {
        return NULL;
    }
    UPyObject *py_switch = new UPyObject(py_switch_);
    PyActuator *actuator = new PyActuator(py_switch);
    self->heater = actuator;
    tempControl.heater = actuator;

    Py_RETURN_NONE;
}

static PyObject *
TempControl_setCooler(TempControl_Object *self, PyObject *args) {
    if(self->cooler != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "cooler already set");
        return NULL;
    }

    PyObject *py_switch_;
    if(!PyArg_ParseTuple(args, "O", &py_switch_)) {
        return NULL;
    }
    UPyObject *py_switch = new UPyObject(py_switch_);
    PyActuator *actuator = new PyActuator(py_switch);
    self->cooler = actuator;
    tempControl.cooler = actuator;

    Py_RETURN_NONE;
}

static PyObject *
TempControl_setMode(PyObject *self, PyObject *args) {
    int mode;
    if(!PyArg_ParseTuple(args, "i", &mode)) {
        return NULL;
    }
    try {
        tempControl.setMode(mode);
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

/*
   Parsing python arguments sucks, this method is used
   by both setBeerTemp and setFridgeTemp to discover the
   temperature that is passed in, mandatory c or f
   */
temperature parseSetTempArgs(TempControl_Object *self, PyObject *args, PyObject *kwds) {
    PyObject *c = NULL;
    PyObject *f = NULL;
    static const char *kwlist[] = {"c", "f", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|$OO", (char **) kwlist, &c, &f)) {
        throw std::exception();
    }                                                                                   

    if(c == NULL && f == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "must specify c or f");
        throw std::exception();
    }
    if(c != NULL && f != NULL) {
        PyErr_SetString(PyExc_RuntimeError, "must specify only c or only f");
        throw std::exception();
    }

    temperature temp;
    if(c != NULL) {
        temp = doubleToTemp(shorten(pyNumToDouble(c)));
    }
    if(f != NULL) {
        temp = doubleToTemp(shorten(f2c(pyNumToDouble(f))));
    }
    return temp;
}

static PyObject *
TempControl_setBeerTemp(TempControl_Object *self, PyObject *args, PyObject *kwds) {
    try {
        temperature temp = parseSetTempArgs(self, args, kwds);;
        tempControl.setBeerTemp(temp);
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_setFridgeTemp(TempControl_Object *self, PyObject *args, PyObject *kwds) {
    try {                                                        
        temperature temp = parseSetTempArgs(self, args, kwds);
        tempControl.setFridgeTemp(temp);
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_reset(PyObject *self, PyObject *args) {
    try {
        tempControl.reset();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_loadDefaultSettings(PyObject *self, PyObject *args) {
    try {
        tempControl.loadDefaultSettings();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_loadDefaultConstants(PyObject *self, PyObject *args) {
    try {
        tempControl.loadDefaultConstants();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_updateTemperatures(PyObject *self, PyObject *args) {
    try {
        tempControl.updateTemperatures();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_detectPeaks(PyObject *self, PyObject *args) {
    try {
        tempControl.detectPeaks();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_updatePID(PyObject *self, PyObject *args) {
    try {
        tempControl.updatePID();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_getState(PyObject *self, PyObject *args) {
    unsigned char state;
    try {
        state = tempControl.getState();
    } catch(...) {
        return NULL;
    }
    return PyLong_FromLong(state);
}

static PyObject *
TempControl_updateState(PyObject *self, PyObject *args) {
    try {
        tempControl.updateState();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_updateOutputs(PyObject *self, PyObject *args) {
    try {
        tempControl.updateOutputs();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *
TempControl_initFilters(PyObject *self, PyObject *args) {
    try {
        tempControl.initFilters();
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

PyObject *getFromDict(PyObject *d, const char *key) {
    PyObject *o = PyDict_GetItemString(d, key);
    if(o == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "key not found");
        throw std::exception();
    }
    return o;
}

static PyObject *
TempControl_setControlSettings(TempControl_Object *self, PyObject *args) {
    PyObject *cs;
    if(!PyArg_ParseTuple(args, "O", &cs)) {
        return NULL;
    }
    if(!PyDict_Check(cs)) {
        PyErr_SetString(PyExc_RuntimeError, "dictionary expected");
        return NULL;
    }
    try {
        
    } catch(...) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static NewRef_PyObject *
TempControl_getControlSettings(TempControl_Object *self, PyObject *args) {
    PPyObject d(PyDict_New());
    PyDict_SetItemString(d, "mode", PPyObject(PyLong_FromLong(tempControl.cs.mode)));
    PyDict_SetItemString(d, "beerSetting", PPyObject(PyFloat_FromDouble(convertTemp(self, tempToDouble(tempControl.cs.beerSetting)))));
    PyDict_SetItemString(d, "fridgeSettings", PPyObject(PyFloat_FromDouble(convertTemp(self, tempToDouble(tempControl.cs.fridgeSetting)))));
    PyDict_SetItemString(d, "heatEstimator", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cs.heatEstimator)))));
    PyDict_SetItemString(d, "coolEstimator", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cs.coolEstimator)))));
    return d;
}

static NewRef_PyObject *
TempControl_getControlVariables(TempControl_Object *self, PyObject *args) {
    PPyObject d(PyDict_New());
    PyDict_SetItemString(d, "beerDiff", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.beerDiff)))));
    PyDict_SetItemString(d, "diffIntegral", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.diffIntegral)))));
    PyDict_SetItemString(d, "beerSlope", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.beerSlope)))));
    PyDict_SetItemString(d, "p", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.p)))));
    PyDict_SetItemString(d, "i", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.i)))));
    PyDict_SetItemString(d, "d", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.d)))));
    PyDict_SetItemString(d, "estimatedPeak", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.estimatedPeak)))));
    PyDict_SetItemString(d, "negPeakEstimate", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.negPeakEstimate)))));
    PyDict_SetItemString(d, "posPeakEstimate", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.posPeakEstimate)))));
    PyDict_SetItemString(d, "negPeak", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.negPeak)))));
    PyDict_SetItemString(d, "posPeak", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cv.posPeak)))));
    return d;
}

static NewRef_PyObject *
TempControl_getControlConstants(TempControl_Object *self, PyObject *args) {
    PPyObject d(PyDict_New());
    PyDict_SetItemString(d, "tempFormats", PPyObject(PyLong_FromLong(tempControl.cc.tempFormat)));
    PyDict_SetItemString(d, "tempSettingMin", PPyObject(PyFloat_FromDouble(convertTemp(self, tempToDouble(tempControl.cc.tempSettingMin)))));
    PyDict_SetItemString(d, "tempSettingMax", PPyObject(PyFloat_FromDouble(convertTemp(self, tempToDouble(tempControl.cc.tempSettingMax)))));
    PyDict_SetItemString(d, "Kp", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.Kp)))));
    PyDict_SetItemString(d, "Ki", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.Ki)))));
    PyDict_SetItemString(d, "Kd", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.Kd)))));
    PyDict_SetItemString(d, "iMaxError", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.iMaxError)))));
    PyDict_SetItemString(d, "idleRangeHigh", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.idleRangeHigh)))));
    PyDict_SetItemString(d, "idleRangeLow", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.idleRangeLow)))));
    PyDict_SetItemString(d, "heatingTargetUpper", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.heatingTargetUpper)))));
    PyDict_SetItemString(d, "heatingTargetLower", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.heatingTargetLower)))));
    PyDict_SetItemString(d, "coolingTargetUpper", PPyObject(PyFloat_FromDouble(convertTempDiff(self ,tempDiffToDouble(tempControl.cc.coolingTargetUpper)))));
    PyDict_SetItemString(d, "coolingTargetLower", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.coolingTargetLower)))));
    PyDict_SetItemString(d, "maxHeatTimeForEstimate", PPyObject(PyLong_FromLong(tempControl.cc.maxHeatTimeForEstimate)));
    PyDict_SetItemString(d, "maxCoolTimeForEstimate", PPyObject(PyLong_FromLong(tempControl.cc.maxCoolTimeForEstimate)));
    PyDict_SetItemString(d, "fridgeFastFilter", PPyObject(PyLong_FromLong(tempControl.cc.fridgeFastFilter)));
    PyDict_SetItemString(d, "fridgeSlowFilter", PPyObject(PyLong_FromLong(tempControl.cc.fridgeSlowFilter)));
    PyDict_SetItemString(d, "fridgeSlopeFilter", PPyObject(PyLong_FromLong(tempControl.cc.fridgeSlopeFilter)));
    PyDict_SetItemString(d, "beerFastFilter", PPyObject(PyLong_FromLong(tempControl.cc.beerFastFilter)));
    PyDict_SetItemString(d, "beerSlowFilter", PPyObject(PyLong_FromLong(tempControl.cc.beerSlowFilter)));
    PyDict_SetItemString(d, "beerSlopeFilter", PPyObject(PyLong_FromLong(tempControl.cc.beerSlopeFilter)));
    PyDict_SetItemString(d, "lightAsHeater", PPyObject(PyLong_FromLong(tempControl.cc.lightAsHeater)));
    PyDict_SetItemString(d, "rotaryHalfSteps", PPyObject(PyLong_FromLong(tempControl.cc.rotaryHalfSteps)));
    PyDict_SetItemString(d, "pidMax", PPyObject(PyFloat_FromDouble(convertTempDiff(self, tempDiffToDouble(tempControl.cc.pidMax)))));
    return d;
}

static PyMethodDef TempControl_Methods[] = {
    {"init", TempControl_init, METH_NOARGS, NULL},
    {"reset", TempControl_reset, METH_NOARGS, NULL},
    {"updateTemperatures", TempControl_updateTemperatures, METH_NOARGS, NULL},
    {"updatePID", TempControl_updatePID, METH_NOARGS, NULL},
    {"getState", TempControl_getState, METH_NOARGS, NULL},
    {"updateState", TempControl_updateState, METH_NOARGS, NULL},
    {"updateOutputs", TempControl_updateOutputs, METH_NOARGS, NULL},
    {"detectPeaks", TempControl_detectPeaks, METH_NOARGS, NULL},
    {"loadDefaultSettings", TempControl_loadDefaultSettings, METH_NOARGS, NULL},
    {"loadDefaultConstants", TempControl_loadDefaultConstants, METH_NOARGS, NULL},
    {"setBeerTemp", (PyCFunction) TempControl_setBeerTemp, METH_VARARGS | METH_KEYWORDS, NULL},
    {"setFridgeTemp", (PyCFunction) TempControl_setFridgeTemp, METH_VARARGS | METH_KEYWORDS, NULL},
    {"setMode", TempControl_setMode, METH_VARARGS, NULL},
    {"setBeerSensor", (PyCFunction) TempControl_setBeerSensor, METH_VARARGS, NULL},
    {"setFridgeSensor", (PyCFunction) TempControl_setFridgeSensor, METH_VARARGS, NULL},
    {"setHeater", (PyCFunction) TempControl_setHeater, METH_VARARGS, NULL},
    {"setCooler", (PyCFunction) TempControl_setCooler, METH_VARARGS, NULL},
    {"initFilters", (PyCFunction) TempControl_initFilters, METH_NOARGS, NULL},
    {"getControlSettings", (PyCFunction) TempControl_getControlSettings, METH_NOARGS, NULL},
    {"getControlVariables", (PyCFunction) TempControl_getControlVariables, METH_NOARGS, NULL},
    {"getControlConstants", (PyCFunction) TempControl_getControlConstants, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject TempControl_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "TempControl.TempControl",             /* tp_name */
    sizeof(TempControl_Object), /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor) TempControl_dealloc__,     /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    "TempControl",           /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    TempControl_Methods,             /* tp_methods */
    0,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc) TempControl_init__,      /* tp_init */
    0,                         /* tp_alloc */
    TempControl_new__,                 /* tp_new */
};

static struct PyModuleDef TempControl_Module = {
    PyModuleDef_HEAD_INIT,
    "TempControl",   /* name of module */
    NULL, /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    NULL //TempControlMethods
};

PyMODINIT_FUNC
PyInit_TempControl(void)
{
    PyObject *module = PyModule_Create(&TempControl_Module);
    if(module == NULL) {
        return NULL;
    }

    if (PyType_Ready(&TempControl_Type) < 0)
        return NULL;
    Py_INCREF(&TempControl_Type);

    PyModule_AddObject(module, "TempControl", (PyObject *) &TempControl_Type);

    PyModule_AddIntConstant(module, "MODE_FRIDGE_CONSTANT", MODE_FRIDGE_CONSTANT);
    PyModule_AddIntConstant(module, "MODE_BEER_CONSTANT", MODE_BEER_CONSTANT);
    PyModule_AddIntConstant(module, "MODE_BEER_PROFILE", MODE_BEER_PROFILE);
    PyModule_AddIntConstant(module, "MODE_OFF", MODE_OFF);
    PyModule_AddIntConstant(module, "MODE_TEST", MODE_TEST);

    return module;
}
