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

/*
   PyBasicTempSensor wraps a BasicTempSensor.  It calls
   into python to find temperature data.

   python interface

   class Sensor:

       def read(unit=[c|f])
   
   */
class PyBasicTempSensor : public BasicTempSensor {

    private:
        CPyObject py_sensor;

    public:
        PyBasicTempSensor(CPyObject py_sensor) {
            this->py_sensor = py_sensor;
        }

        bool isConnected(void) {
            return true;
        }

        bool init(void) {
        }

        temperature read() {
            CPyObject m(PyObject_GetAttrString(this->py_sensor, "read"));
            CPyObject kw(PyDict_New());
            CPyObject unit(PyUnicode_FromString("c"));
            PyDict_SetItemString(kw, "unit", unit);
            CPyObject args(PyTuple_New(0));
            CPyObject r(PyObject_Call(m, args, kw));
            
            temperature temp;
            if(r == Py_None) {
                temp = TEMP_SENSOR_DISCONNECTED;
            } else {
                temp = pyNumToTemp('c', r);
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
        CPyObject py_switch;

    public:
        PyActuator(CPyObject py_switch) {
            this->py_switch = py_switch;
        }

        void setActive(bool active) {
            CPyObject m;
            if(active) {
                m.reset(PyObject_GetAttrString(this->py_switch, "on"));
            } else {
                m.reset(PyObject_GetAttrString(this->py_switch, "off"));
            }
            CPyObject r(PyObject_CallObject(m, NULL)); 
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

class TempControlRefs {
    public:
        std::unique_ptr<BasicTempSensor> basicBeerSensor;
        std::unique_ptr<TempSensor> beerSensor;
        std::unique_ptr<BasicTempSensor> basicFridgeSensor;
        std::unique_ptr<TempSensor> fridgeSensor;
        std::unique_ptr<PyActuator> heater;
        std::unique_ptr<PyActuator> cooler;
};

typedef struct {
    PyObject_HEAD
    TempControlRefs *refs;
    char unit;
} TempControl_Object;

static void
TempControl_dealloc__(TempControl_Object *self) {
    delete(self->refs);
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
    self->refs = new TempControlRefs();

    initialized = true;

    return (PyObject *) self;
}

static int TempControl_init__(TempControl_Object *self, PyObject *args, PyObject *kwds) {
    try {
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

        unit = unit;

        return 0;
    } catch(...) {
        return -1;
    }
}

/*
   Different from python __init__, this method performs some
   basic initialization of the tempControl object
   */
static PyObject *
TempControl_init(PyObject *self, PyObject *args) {
    try {
        tempControl.init();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_setBeerSensor(TempControl_Object *self, PyObject *args) {
    try {
        PyObject *py_sensor_;
        if(!PyArg_ParseTuple(args, "O", &py_sensor_)) {
            return NULL;
        }
        CPyObject py_sensor(py_sensor_, true);
        auto basicSensor = std::make_unique<PyBasicTempSensor>(py_sensor);
        auto sensor = std::make_unique<TempSensor>(TEMP_SENSOR_TYPE_BEER, basicSensor.get());

        sensor->init();

        self->refs->basicBeerSensor = std::move(basicSensor);
        self->refs->beerSensor = std::move(sensor);
        tempControl.beerSensor = self->refs->beerSensor.get();

        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_setFridgeSensor(TempControl_Object *self, PyObject *args) {
    try {
        PyObject *py_sensor_;
        if(!PyArg_ParseTuple(args, "O", &py_sensor_)) {
            return NULL;
        }
        CPyObject py_sensor(py_sensor_, true);
        auto basicSensor = std::make_unique<PyBasicTempSensor>(py_sensor);
        auto sensor = std::make_unique<TempSensor>(TEMP_SENSOR_TYPE_BEER, basicSensor.get());

        sensor->init();

        self->refs->basicFridgeSensor = std::move(basicSensor);
        self->refs->fridgeSensor = std::move(sensor);
        tempControl.fridgeSensor = sensor.get();

        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_setHeater(TempControl_Object *self, PyObject *args) {
    try {
        PyObject *py_switch_;
        if(!PyArg_ParseTuple(args, "O", &py_switch_)) {
            return NULL;
        }
        CPyObject py_switch(py_switch_, true);
        self->refs->heater = std::make_unique<PyActuator>(py_switch);
        tempControl.heater = self->refs->heater.get();

        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_setCooler(TempControl_Object *self, PyObject *args) {
    try {
        PyObject *py_switch_;
        if(!PyArg_ParseTuple(args, "O", &py_switch_)) {
            return NULL;
        }
        CPyObject py_switch(py_switch_, true);
        self->refs->cooler = std::make_unique<PyActuator>(py_switch);
        tempControl.cooler = self->refs->cooler.get();

        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_setMode(PyObject *self, PyObject *args) {
    try {
        int mode;
        if(!PyArg_ParseTuple(args, "i", &mode)) {
            return NULL;
        }
        tempControl.setMode(mode);
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
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
        temp = pyNumToTemp('c', c);
    }
    if(f != NULL) {
        temp = pyNumToTemp('f', f);
    }
    return temp;
}

static PyObject *
TempControl_setBeerTemp(TempControl_Object *self, PyObject *args, PyObject *kwds) {
    try {
        temperature temp = parseSetTempArgs(self, args, kwds);;
        tempControl.setBeerTemp(temp);
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_setFridgeTemp(TempControl_Object *self, PyObject *args, PyObject *kwds) {
    try {                                                        
        temperature temp = parseSetTempArgs(self, args, kwds);
        tempControl.setFridgeTemp(temp);
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_reset(PyObject *self, PyObject *args) {
    try {
        tempControl.reset();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_loadDefaultSettings(PyObject *self, PyObject *args) {
    try {
        tempControl.loadDefaultSettings();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_loadDefaultConstants(PyObject *self, PyObject *args) {
    try {
        tempControl.loadDefaultConstants();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_updateTemperatures(PyObject *self, PyObject *args) {
    try {
        tempControl.updateTemperatures();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_detectPeaks(PyObject *self, PyObject *args) {
    try {
        tempControl.detectPeaks();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_updatePID(PyObject *self, PyObject *args) {
    try {
        tempControl.updatePID();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_getState(PyObject *self, PyObject *args) {
    try {
        unsigned char state = tempControl.getState();
        return PyLong_FromLong(state);
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_updateState(PyObject *self, PyObject *args) {
    try {
        tempControl.updateState();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_updateOutputs(PyObject *self, PyObject *args) {
    try {
        tempControl.updateOutputs();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_initFilters(PyObject *self, PyObject *args) {
    try {
        tempControl.initFilters();
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_setControlSettings(TempControl_Object *self, PyObject *args) {
    try {
        PyObject *cs;
        if(!PyArg_ParseTuple(args, "O", &cs)) {
            return NULL;
        }
        if(!PyDict_Check(cs)) {
            PyErr_SetString(PyExc_RuntimeError, "dictionary expected");
            return NULL;
        }
        char unit = unit;
        tempControl.cs.mode = pyNumToLong(getFromDict(cs, "mode"));
        tempControl.cs.beerSetting = pyNumToTemp(unit, getFromDict(cs, "beerSetting"));
        tempControl.cs.fridgeSetting = pyNumToTemp(unit, getFromDict(cs, "fridgeSettings"));
        tempControl.cs.heatEstimator = pyNumToTempDiff(unit, getFromDict(cs, "heatEstimator"));
        tempControl.cs.coolEstimator = pyNumToTempDiff(unit, getFromDict(cs, "coolEstimator"));
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_setControlVariables(TempControl_Object *self, PyObject *args) {
    try {
        PyObject *cv;
        if(!PyArg_ParseTuple(args, "O", &cv)) {
            return NULL;
        }
        if(!PyDict_Check(cv)) {
            PyErr_SetString(PyExc_RuntimeError, "dictionary expected");
            return NULL;
        }
        char unit = unit;
        tempControl.cv.beerDiff = pyNumToTempDiff(unit, getFromDict(cv, "beerDiff"));
        tempControl.cv.diffIntegral = pyNumToTempDiff(unit, getFromDict(cv, "diffIntegral"));
        tempControl.cv.beerSlope = pyNumToTempDiff(unit, getFromDict(cv, "beerSlope"));
        tempControl.cv.p = pyNumToTempDiff(unit, getFromDict(cv, "p"));
        tempControl.cv.i = pyNumToTempDiff(unit, getFromDict(cv, "i"));
        tempControl.cv.d = pyNumToTempDiff(unit, getFromDict(cv, "d"));
        tempControl.cv.estimatedPeak = pyNumToTempDiff(unit, getFromDict(cv, "estimatedPeak"));
        tempControl.cv.negPeakEstimate = pyNumToTempDiff(unit, getFromDict(cv, "negPeakEstimate"));
        tempControl.cv.posPeakEstimate = pyNumToTempDiff(unit, getFromDict(cv, "posPeakEstimate"));
        tempControl.cv.negPeak = pyNumToTempDiff(unit, getFromDict(cv, "negPeak"));
        tempControl.cv.posPeak = pyNumToTempDiff(unit, getFromDict(cv, "posPeak"));
        Py_RETURN_NONE;
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_getControlSettings(TempControl_Object *self, PyObject *args) {
    try {
        CPyObject d(PyDict_New());
        char unit = self->unit;
        PyDict_SetItemString(d, "mode", CPyObject(PyLong_FromLong(tempControl.cs.mode)));
        PyDict_SetItemString(d, "beerSetting", tempToPyFloat(unit, tempControl.cs.beerSetting));
        PyDict_SetItemString(d, "fridgeSetting", tempToPyFloat(unit, tempControl.cs.fridgeSetting));
        PyDict_SetItemString(d, "heatEstimator", tempDiffToPyFloat(unit, tempControl.cs.heatEstimator));
        PyDict_SetItemString(d, "coolEstimator", tempDiffToPyFloat(unit, tempControl.cs.coolEstimator));
        return d.release();
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_getControlVariables(TempControl_Object *self, PyObject *args) {
    try {
        CPyObject d(PyDict_New());
        char unit = self->unit;
        PyDict_SetItemString(d, "beerDiff", tempDiffToPyFloat(unit, tempControl.cv.beerDiff));
        PyDict_SetItemString(d, "diffIntegral", tempDiffToPyFloat(unit, tempControl.cv.diffIntegral));
        PyDict_SetItemString(d, "beerSlope", tempDiffToPyFloat(unit, tempControl.cv.beerSlope));
        PyDict_SetItemString(d, "p", tempDiffToPyFloat(unit, tempControl.cv.p));
        PyDict_SetItemString(d, "i", tempDiffToPyFloat(unit, tempControl.cv.i));
        PyDict_SetItemString(d, "d", tempDiffToPyFloat(unit, tempControl.cv.d));
        PyDict_SetItemString(d, "estimatedPeak", tempDiffToPyFloat(unit, tempControl.cv.estimatedPeak));
        PyDict_SetItemString(d, "negPeakEstimate", tempDiffToPyFloat(unit, tempControl.cv.negPeakEstimate));
        PyDict_SetItemString(d, "posPeakEstimate", tempDiffToPyFloat(unit, tempControl.cv.posPeakEstimate));
        PyDict_SetItemString(d, "negPeak", tempDiffToPyFloat(unit, tempControl.cv.negPeak));
        PyDict_SetItemString(d, "posPeak", tempDiffToPyFloat(unit, tempControl.cv.posPeak));
        return d.release();
    } catch(...) {
        return NULL;
    }
}

static PyObject *
TempControl_getControlConstants(TempControl_Object *self, PyObject *args) {
    try {
        CPyObject d(PyDict_New());
        char unit = self->unit;
        PyDict_SetItemString(d, "tempFormats", CPyObject(PyLong_FromLong(tempControl.cc.tempFormat)));
        PyDict_SetItemString(d, "tempSettingMin", tempToPyFloat(unit, tempControl.cc.tempSettingMin));
        PyDict_SetItemString(d, "tempSettingMax", tempToPyFloat(unit, tempControl.cc.tempSettingMax));
        PyDict_SetItemString(d, "Kp", tempDiffToPyFloat(unit, tempControl.cc.Kp));
        PyDict_SetItemString(d, "Ki", tempDiffToPyFloat(unit, tempControl.cc.Ki));
        PyDict_SetItemString(d, "Kd", tempDiffToPyFloat(unit, tempControl.cc.Kd));
        PyDict_SetItemString(d, "iMaxError", tempDiffToPyFloat(unit, tempControl.cc.iMaxError));
        PyDict_SetItemString(d, "idleRangeHigh", tempDiffToPyFloat(unit, tempControl.cc.idleRangeHigh));
        PyDict_SetItemString(d, "idleRangeLow", tempDiffToPyFloat(unit, tempControl.cc.idleRangeLow));
        PyDict_SetItemString(d, "heatingTargetUpper", tempDiffToPyFloat(unit, tempControl.cc.heatingTargetUpper));
        PyDict_SetItemString(d, "heatingTargetLower", tempDiffToPyFloat(unit, tempControl.cc.heatingTargetLower));
        PyDict_SetItemString(d, "coolingTargetUpper", tempDiffToPyFloat(unit, tempControl.cc.coolingTargetUpper));
        PyDict_SetItemString(d, "coolingTargetLower", tempDiffToPyFloat(unit, tempControl.cc.coolingTargetLower));
        PyDict_SetItemString(d, "maxHeatTimeForEstimate", CPyObject(PyLong_FromLong(tempControl.cc.maxHeatTimeForEstimate)));
        PyDict_SetItemString(d, "maxCoolTimeForEstimate", CPyObject(PyLong_FromLong(tempControl.cc.maxCoolTimeForEstimate)));
        PyDict_SetItemString(d, "fridgeFastFilter", CPyObject(PyLong_FromLong(tempControl.cc.fridgeFastFilter)));
        PyDict_SetItemString(d, "fridgeSlowFilter", CPyObject(PyLong_FromLong(tempControl.cc.fridgeSlowFilter)));
        PyDict_SetItemString(d, "fridgeSlopeFilter", CPyObject(PyLong_FromLong(tempControl.cc.fridgeSlopeFilter)));
        PyDict_SetItemString(d, "beerFastFilter", CPyObject(PyLong_FromLong(tempControl.cc.beerFastFilter)));
        PyDict_SetItemString(d, "beerSlowFilter", CPyObject(PyLong_FromLong(tempControl.cc.beerSlowFilter)));
        PyDict_SetItemString(d, "beerSlopeFilter", CPyObject(PyLong_FromLong(tempControl.cc.beerSlopeFilter)));
        PyDict_SetItemString(d, "lightAsHeater", CPyObject(PyLong_FromLong(tempControl.cc.lightAsHeater)));
        PyDict_SetItemString(d, "rotaryHalfSteps", CPyObject(PyLong_FromLong(tempControl.cc.rotaryHalfSteps)));
        PyDict_SetItemString(d, "pidMax", tempDiffToPyFloat(unit, tempControl.cc.pidMax));
        return d.release();
    } catch(...) {
        return NULL;
    }
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
    {"setControlSettings", (PyCFunction) TempControl_setControlSettings, METH_VARARGS, NULL},
    {"getControlVariables", (PyCFunction) TempControl_getControlVariables, METH_NOARGS, NULL},
    {"setControlVariables", (PyCFunction) TempControl_setControlVariables, METH_VARARGS, NULL},
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
