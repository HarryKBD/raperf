C++ = g++

ifndef os
   os = LINUX
endif

ifndef arch
   arch = IA64
endif
CCFLAGS = -fPIC -Wall -I../src -Wextra -O3 -finline-functions -O3 -fno-strict-aliasing -fvisibility=hidden
 
ifeq ($(arch), IA64)
    CCFLAGS += -DIA64
endif

ifeq ($(arch), ARM7)
    CCFLAGS += -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mneon-for-64bits
endif

LDFLAGS = ../src/libudt.a -lstdc++ -lpthread -lm

ifeq ($(os), UNIX)
   LDFLAGS += -lsocket
endif

ifeq ($(os), SUNOS)
   LDFLAGS += -lrt -lsocket
endif

DIR = $(shell pwd)

APP = appserver appclient sendfile recvfile test perfserver perfclient

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
test: test.o
	$(C++) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o $(APP)

install:
	export PATH=$(DIR):$$PATH
