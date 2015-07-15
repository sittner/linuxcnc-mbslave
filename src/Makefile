include ../config.mk

EXTRA_CFLAGS := $(filter-out -Wframe-larger-than=%,$(EXTRA_CFLAGS))

OBJS = mbslave_main.o mbslave_util.o mbslave_conf.o mbslave_tcp.o mbslave_prot.o

.PHONY: test all clean

all: mbslave

%.o: %.c
	$(CC) -o $@ $(EXTRA_CFLAGS) -URTAPI -U__MODULE__ -DULAPI -Os -c $<

mbslave: $(OBJS)
	$(CC) -o $@ $(OBJS) -Wl,-rpath,$(LIBDIR) -L$(LIBDIR) -llinuxcnchal -lexpat -lpthread

install: mbslave
	mkdir -p $(DESTDIR)$(EMC2_HOME)/bin
	cp mbslave $(DESTDIR)$(EMC2_HOME)/bin/

clean:
	rm -f *.o mbslave

