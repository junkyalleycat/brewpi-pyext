#!/usr/bin/python3

import sys
sys.path.append('build')
import TempControl
import time

class FermLoop:

    def run(self, target, fermTemp, fridgeTemp, heater, cooler):
        tempControl = TempControl.TempControl(unit='f')
        tempControl.setBeerSensor(fermTemp)
        tempControl.setFridgeSensor(fridgeTemp)
        tempControl.setHeater(heater)
        tempControl.setCooler(cooler)
        tempControl.init()
        tempControl.loadDefaultSettings()
        tempControl.loadDefaultConstants()
        tempControl.setMode(TempControl.MODE_BEER_PROFILE)
        tempControl.setBeerTemp(f=target)
        tempControl.setFridgeTemp(f=target)

        nextSampleTime = time.time() + 0
        while(True):
            time.sleep(max(0, nextSampleTime - time.time()))
            nextSampleTime += 1

            tempControl.updateTemperatures()
            tempControl.detectPeaks()
            tempControl.updatePID()
            oldState = tempControl.getState()
            tempControl.updateState()
            if(oldState != tempControl.getState()):
                c.print("state change")
            tempControl.updateOutputs() # doesn't really do anything
            fermvars = {}
            cs = tempControl.getControlSettings()
            for k in cs.keys():
                fermvars['cs.%s' % k] = cs[k]
            cv = tempControl.getControlVariables()
            for k in cv.keys():
                fermvars['cv.%s' % k] = cv[k]
            print(fermvars)

class Temp:

    def __init__(self, c):
        self.c = c

    def read(self, unit=None):
        if(unit == 'c'):
            return self.c 
        elif(unit == 'f'):
            return (self.c * 1.8) + 32
        else:
            raise Exception("Unknown unit: %s" % unit)

class Switch:

    def __init__(self, name):
        self.name = name

    def on(self):
        print("%s on" % self.name)

    def off(self):
        print("%s off" % self.name)
                
FermLoop().run(68, Temp(70), Temp(66), Switch("heater"), Switch("cooler"))
