#pragma once

// This class makes handling local python objects easier
// at it moves their decref to the destructor           
#include <Python.h>

class CPyObject {
    private:
        PyObject *o = nullptr;

        void _reset(PyObject *o, bool protect) {
            if(this->o != nullptr) {
                Py_XDECREF(this->o);
            }
            if(protect && (o != nullptr)) {
                Py_XINCREF(o);
            }
            this->o = o;
        }

    public:
        CPyObject() {
            reset();
        }

        CPyObject(PyObject *o) {
            reset(o);
        }
        
        CPyObject(PyObject *o, bool protect) {
            reset(o, protect);
        }

        ~CPyObject() {
            reset();
        }

        PyObject *release() {
            PyObject *o = this->o;
            this->o = nullptr;
            return o;
        }

        void reset() {
            _reset(nullptr, false);
        }

        void reset(PyObject *o) {
            reset(o, false);
        }

        void reset(PyObject *o, bool protect) {
            if(o == NULL) {
                throw std::exception();
            }
            _reset(o, protect);
        }

        operator PyObject *() const {
            return o;
        }

        // move
        CPyObject(CPyObject &&that) noexcept {
            _reset(that.release(), false);
        }

        CPyObject& operator =(CPyObject &&that) noexcept {
            if(this != &that) {
                _reset(that.release(), false);
            }
            return *this;
        }

        // copy
        CPyObject(const CPyObject &that) {
            _reset(that.o, true);
        }

        CPyObject& operator =(const CPyObject &that) {
            if(this != &that) {
                _reset(that.o, true);
            }
            return *this;
        }
};
