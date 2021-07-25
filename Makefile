OPTS:=$(OPTS) -g3 -lSegFault -module completion
CHARMC=$(CHARM_HOME)/bin/charmc $(OPTS)

INCLUDES = -I$(HYPERCOMM_HOME)/include -I$(HYPERCOMM_HOME)/include/hypercomm/core
LIBS = -L$(HYPERCOMM_HOME)/lib -lhypercomm-components -lhypercomm-utilities -lhypercomm-serialization -lhypercomm-messaging -lSegFault

OBJS = hello.o

ifndef HYPERCOMM_HOME
	$(error HYPERCOMM_HOME is undefined)
endif

all: hello

hello: $(OBJS)
	$(CHARMC) -language charm++ -o hello $(OBJS) $(LIBS)

tree_builder.decl.h: tree_builder.ci
	$(CHARMC) tree_builder.ci

hello.decl.h: hello.ci tree_builder.decl.h
	$(CHARMC) hello.ci

clean:
	rm -f *.decl.h *.def.h conv-host *.o hello charmrun

hello.o: hello.cc hello.decl.h manage*.hh tree_builder.hh common.hh
	$(CHARMC) -c hello.cc $(INCLUDES)

test: all
	./charmrun ++local ++autoProvision $(shell which catchsegv) ./hello
