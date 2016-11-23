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

/*
   It seems that the TempControl code has mostly
   been excercised using celsius, so
   i won't use the format variable in
   TempControl, but will use my own that
   modifies temp at the gate
   */
double c2f(double c);
double f2c(double f);
double tempToDouble(temperature val);
double tempDiffToDouble(temperature val);
temperature doubleToTempDiff(double f);
/*
   Because a number from python might be a float or an int
   */
double pyNumToDouble(PyObject *pyNum);
long pyNumToLong(PyObject *pyNum);
/*
   shortens a double, i was getting strange results
   before i did this, don't care to investigate
   */
double shortenDouble(double v);
double convertToTemp(char unit, double temp_c);
double convertFromTemp(char unit, double temp);
double convertToTempDiff(char unit, double temp_c);
double convertFromTempDiff(char unit, double temp);
void pyerr_printf(const char *format, ...);
// borrowedref
PyObject *getFromDict(PyObject *d, const char *key);
temperature pyNumToTemp(char unit, PyObject *n);
temperature pyNumToTempDiff(char unit, PyObject *n);
// newref
PyObject *tempToPyFloat(char unit, temperature t);
// newref
PyObject *tempDiffToPyFloat(char unit, temperature t);