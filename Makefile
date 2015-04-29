default: HearThereOSC


SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.o)
LIBS=-llo -lbluetooth

HearThereOSC: $(OBJECTS)
	gcc -o$@ $(OBJECTS) $(LIBS)

clean:
	rm -rf $(OBJECTS) HearThereOSC
