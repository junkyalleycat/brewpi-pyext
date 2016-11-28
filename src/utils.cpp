/**
  The python glue code is entirely contained in this file.  The code
  is pretty streightforward, define a new python type (TempControl) and
  provide it with all relavent methods of TempControl.cpp.  The methods
  in this class are passthroughs and do very little except some unit
  translation.

  There are a few methods that need to be defined but aren't used, these
  are included in a block towards the top of this file.
  */

#include <stdio.h>
#include <Python.h>
#include <stdexcept>
#include "cpy.h"
#include "TemperatureFormats.h"

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

temperature doubleToTempDiff(double f) {
    return f * TEMP_FIXED_POINT_SCALE;
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

long pyNumToLong(PyObject *pyNum) {
    long val;
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

double internalToUnit(char unit, double temp_c) {
    if(unit == 'c') {
        return temp_c;
    }
    return c2f(temp_c);
}

double unitToInternal(char unit, double temp) {
    if(unit == 'c') {
        return temp;
    }
    return f2c(temp);
}

double internalDiffToUnit(char unit, double temp_c) {
    if(unit == 'c') {
        return temp_c;
    }
    return c2f(temp_c) - 32;
}

double unitToInternalDiff(char unit, double temp) {
    if(unit == 'c') {
        return temp;
    }
    return f2c(temp + 32);
}

temperature pyNumToTemp(char unit, PyObject *n) {
    return doubleToTemp(unitToInternal(unit, pyNumToDouble(n)));
}

temperature pyNumToTempDiff(char unit, PyObject *n) {
    return doubleToTempDiff(unitToInternalDiff(unit, pyNumToDouble(n)));
}

CPyObject tempToPyFloat(char unit, temperature t) {
    return CPyObject(PyFloat_FromDouble(internalToUnit(unit, tempToDouble(t))));
}

CPyObject tempDiffToPyFloat(char unit, temperature t) {
    return CPyObject(PyFloat_FromDouble(internalDiffToUnit(unit, tempDiffToDouble(t))));
}

void pyerr_printf(const char *format, ...) {
    char buffer[128];
    va_list args;
    va_start (args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end (args);
    CPyObject err(PyUnicode_FromString(buffer));
    PyErr_SetObject(PyExc_RuntimeError, err);
}

CPyObject getFromDict(PyObject *d, const char *key) {
    PyObject *o = PyDict_GetItemString(d, key);
    if(o == NULL) {
        pyerr_printf("Key not found: %s", key);  
        throw std::exception();
    }
    return CPyObject(o, true);
}
