default: rfduino_osc


SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.o)
LIBS=-llo -lbluetooth

rfduino_osc: $(OBJECTS)
	gcc -orfduino_osc $(LIBS)

