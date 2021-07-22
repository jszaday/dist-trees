OPTS:=$(OPTS) -g3 -lSegFault
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)

INCLUDES = -I$(HYPERCOMM_HOME)/include -I$(HYPERCOMM_HOME)/include/hypercomm/core
LIBS = -L$(HYPERCOMM_HOME)/lib -lhypercomm-components -lhypercomm-utilities -lhypercomm-serialization -lhypercomm-messaging -lSegFault

OBJS = hello.o

all: hello

hello: check-env $(OBJS)
	$(CHARMC) -language charm++ -o hello $(OBJS) $(LIBS)

hello.decl.h: hello.ci
	$(CHARMC) hello.ci

clean:
	rm -f *.decl.h *.def.h conv-host *.o hello charmrun

hello.o: check-env hello.cc hello.decl.h nlocmgr.hh
	$(CHARMC) -c hello.cc $(INCLUDES)

test: all
	./charmrun ++local ++autoProvision $(shell which catchsegv) ./hello

check-env:
ifndef HYPERCOMM_HOME
	$(error HYPERCOMM_HOME is undefined)
endif
