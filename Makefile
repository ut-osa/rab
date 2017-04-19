CC = gcc
CXX = g++

ARCH ?= i386

ifeq "$(ARCH)" "i386"
ARCH_OPT := -m32
else ifeq "$(ARCH)" "x86_64"
ARCH_OPT := -m64
else
ARCH_OPT = $(error "not supported architecture")
endif

all: rab rab-tx rab-tx-pre-xend rab-big-tx

rab: rab.c
	$(CC) -g $(ARCH_OPT) -o rab rab.c

rab-tx: rab.c
	$(CC) -g $(ARCH_OPT) -DTX -o rab-tx rab.c

rab-big-tx: rab.c
	$(CC) -g $(ARCH_OPT) -DTX -DBIG_TX -o rab-big-tx rab.c

rab-tx-pre-xend: rab.c
	$(CC) -g $(ARCH_OPT) -DTX -DPRE_XEND_TIME -o rab-tx-pre-xend rab.c

clean: 
	rm -f rab rab-tx rab-tx-pre-xend *.o *~

clean-all: clean
	rm -rf d* srcdir
