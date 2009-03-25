EE_BIN = ../bin/InfoGB.elf

EE_CC = ee-gcc
EE_CPP = ee-gcc
CC = ee-gcc
EE_CXX = ee-gcc

IRX_DIR = ./irx

ZLIB = ./lib/testz/zlib
UNZLIB = ./lib/testz/unzip

EE_CXXFLAGS = -G0 -O2 -I$(NEWLIB)/include -I./gfxpipe -I./ 
EE_CFLAGS = -G0 -O2 -I$(NEWLIB)/include -I./gfxpipe -I./ 
#EE_CXXFLAGS += -finline-functions -fstrict-aliasing  -funsigned-char  -DLSB_FIRST -fomit-frame-pointer 
EE_CFLAGS += -finline-functions -fstrict-aliasing  -funsigned-char  -DLSB_FIRST -fomit-frame-pointer 

EE_LIBS += ./lib/libcdvd/lib/libcdvd2.a -lgcc -lm -lc -lmc -lpad  -I. -lc -lstdc++ -lfileXio -lpatches
EE_OBJS  = $(ZLIB)/adler32.o $(ZLIB)/compress.o $(ZLIB)/crc32.o $(ZLIB)/gzio.o $(ZLIB)/uncompr.o      \
           $(ZLIB)/infcodes.o $(ZLIB)/trees.o $(ZLIB)/deflate.o $(ZLIB)/inflate.o $(ZLIB)/infblock.o  \
           $(ZLIB)/inftrees.o $(ZLIB)/infutil.o $(ZLIB)/inffast.o $(ZLIB)/zutil.o $(UNZLIB)/explode.o \
           $(UNZLIB)/unreduce.o $(UNZLIB)/unshrink.o $(UNZLIB)/unzip.o

EE_OBJS += ./../mem.o ./../cpu.o ./../joypad.o ./../rom.o ./../sound.o ./../vram.o      \
           ./usbd.o ./usbhdfsd.o ./libcdvd.o ./hw.o ./gs.o ./aio-fio.o ./sjpcm.o        \
           ./sjpcm_rpc.o ./fontmsx.o ./gfxpipe/gfxpipe.o ./ps2print.o ./ps2input.o      \
           ./ps2init.o ./ps2gui.o ./ps2romselect.o ./InfoGB_System_ps2.o ./Reboot_ELF.o \
           ./mass.o 

all: $(EE_BIN)
	#ee-strip $(EE_BIN)

sjpcm.s:
	bin2s $(IRX_DIR)/sjpcm.irx sjpcm.s sjpcm_irx

libcdvd.s:
	bin2s $(IRX_DIR)/cdvd.irx libcdvd.s libcdvd_irx

usbd.s:
	bin2s $(IRX_DIR)/usbd.irx usbd.s usbd_irx

usbhdfsd.s:
	bin2s $(IRX_DIR)/usbhdfsd.irx usbhdfsd.s usbhdfsd_irx

clean:
	rm ./libcdvd.s ./sjpcm.s ./usbd.s ./usbhdfsd.s *.o ../*.o ./lib/testz/zlib/*.o ./gfxpipe/*.o

pack:
	ps2-packer-lite $(EE_BIN) $(EE_BIN)-packed

release: clean all pack

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
