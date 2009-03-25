EE_BIN = example.elf
EE_OBJS = main.o

EE_LDFLAGS += -L../lib -L../lib/libcdvd2.a
EE_LIBS += -lcdvd2 

all: $(EE_BIN)

clean:
	rm -f *.elf *.o *.a

include $(PS2LIB)/Makefile.pref
include $(PS2LIB)/Makefile.eeglobal
