##############################
# spcaview Makefile
##############################

INSTALLROOT=$(PWD)

#CC=$(CROSS_COMPILE)gcc
#CPP=$(CROSS_COMPILE)g++
INSTALL=install
APP_BINARY=camview
BIN=/usr/local/bin
MATH_LIB = -lm
#LIBX11FLAGS= -I/usr/X11R6/include -L/usr/X11R6/lib
VERSION = 0.0.1

#WARNINGS = -Wall \
#           -Wundef -Wpointer-arith -Wbad-function-cast \
#           -Wcast-align -Wwrite-strings -Wstrict-prototypes \
#           -Wmissing-prototypes -Wmissing-declarations \
#           -Wnested-externs -Winline -Wcast-qual -W \
#           -Wno-unused
#           -Wunused

CFLAGS += -O0 -g -DLINUX -DVERSION=\"$(VERSION)\" $(WARNINGS)
CPPFLAGS = $(CFLAGS)

OBJECTS= cam_view.o color.o utils.o v4l2uvc.o avilib.o shell.o
		

all:	camview

clean:
	@echo "Cleaning up directory."
	rm -f *.a *.o $(APP_BINARY) core *~ log errlog *.avi

# Applications:
camview:	$(OBJECTS)
	$(CC)	$(CFLAGS) $(OBJECTS) $(X11_LIB) $(XPM_LIB)\
		$(MATH_LIB) \
		-o $(APP_BINARY)
	chmod 755 $(APP_BINARY)


install: camview
	$(INSTALL) -s -m 755 -g root -o root $(APP_BINARY) $(BIN) 
	rm -f $(BIN)/camview
