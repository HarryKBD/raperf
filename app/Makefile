C++ = g++

ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA64
endif

C_INCLUDES = -I/usr/include/cairo -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng12 -I/usr/include/SDL2 -I/usr/include/gtk-3.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/gio-unix-2.0 -I/usr/include/mirclient -I/usr/include/mircommon -I/usr/include/mircookie -I/usr/include/pango-1.0 -I/usr/include/harfbuzz -I/usr/include/atk-1.0 -I/usr/include/gdk-pixbuf-2.0

CCFLAGS = -fPIC -Wall -I../src $(C_INCLUDES) -Wextra -O3 -finline-functions -O3 -fno-strict-aliasing -fvisibility=hidden -g
 
ifeq ($(arch), IA64)
    CCFLAGS += -DIA64
endif

ifeq ($(arch), ARM7)
    CCFLAGS += -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mneon-for-64bits
endif

LDFLAGS = ../src/libudt.a -lstdc++ -lpthread -lm -lcairo -lX11 -lXext -lgtk-3 -lgdk-3 -lGLU -lGL -lcairo -lSDL2 -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -latk-1.0 -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lpthread -lXext -lX11 -lSDL2 -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -latk-1.0 -lcairo-gobject -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lpthread -lXext -lX11 

ifeq ($(os), UNIX)
   LDFLAGS += -lsocket
endif

ifeq ($(os), SUNOS)
   LDFLAGS += -lrt -lsocket
endif

DIR = $(shell pwd)

APP = appserver appclient sendfile recvfile test perfserver perfclient fluxpisvr raw

all: $(APP)

%.o: %.cpp
	$(C++) $(CCFLAGS) $< -c


perfserver: perfserver.o
	$(C++) $^ -o $@ $(LDFLAGS)
perfclient: perfclient.o
	$(C++) $^ -o $@ $(LDFLAGS)
appserver: appserver.o
	$(C++) $^ -o $@ $(LDFLAGS)
appclient: appclient.o
	$(C++) $^ -o $@ $(LDFLAGS)
sendfile: sendfile.o
	$(C++) $^ -o $@ $(LDFLAGS)
recvfile: recvfile.o
	$(C++) $^ -o $@ $(LDFLAGS)
fluxpisvr: fluxpisvr.o buf.o debayer.o cairo_display.o
	$(C++) $^ -o $@ $(LDFLAGS)
raw: raw.o fluxpiclient.o
	$(C++) $^ -o $@ $(LDFLAGS)
test: test.o
	$(C++) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o $(APP)

install:
	export PATH=$(DIR):$PATH
