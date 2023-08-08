CC=g++
CXX=g++
RANLIB=ranlib

CLIENTLIBSRC=client.cpp
CLIENTLIBOBJ=$(LIBSRC:.cpp=.o)

SERVERLIBSRC=server.cpp
SERVERLIBOBJ=$(LIBSRC:.cpp=.o)

INCS=-I.
CFLAGS = -Wall -std=c++11 -g -pthread $(INCS)
CXXFLAGS = -Wall -std=c++11 -g -pthread $(INCS)

CLIENTLIB = libclient.a
SERVERLIB = libserver.a
TARGETS = $(CLIENTLIB) $(SERVERLIB)

TAR=tar
TARFLAGS=-cvf
TARNAME=ex5.tar
TARSRCS= $(CLIENTLIBSRC) $(SERVERLIBSRC) ANSWERS.txt Makefile README

all: $(TARGETS)

$(CLIENTLIB): $(CLIENTOBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

$(SERVERLIB): $(SERVEROBJ)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	$(RM) $(TARGETS) $(CLIENTOBJ) $(SERVEROBJ) $(OBJ) $(LIBOBJ) *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(CLIENTSRC) $(SERVERSRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
