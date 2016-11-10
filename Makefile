CC=gcc
CFLAGS=-g -Ilinks -Isrc -fpermissive $(shell pkg-config --cflags python3)
LIBS=-Wl,--no-undefined -lstdc++ $(shell pkg-config --libs python3)
SRC=src/glue.cpp
LINKSRC=links/Actuator.cpp links/FilterCascaded.cpp links/FilterFixed.cpp links/Sensor.cpp links/TempSensor.cpp links/TempControl.cpp links/Ticks.cpp links/TemperatureFormats.cpp
OBJS=$(SRC:src/%.cpp=build/%.o) $(LINKSRC:links/%.cpp=build/%.o)

build/%.o: links/%.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: src/%.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

build/TempControl.so: $(OBJS)
	gcc -shared -o $@ $^ $(LIBS)

clean:
	rm build/*
