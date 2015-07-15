.PHONY: all configure install clean

all: configure
	@$(MAKE) -C src all

clean:
	rm -f src/*.o src/mbslave
	rm -f config.mk config.mk.tmp

install: configure
	@$(MAKE) -C src install

configure: config.mk

config.mk: configure.mk
	@$(MAKE) -s -f configure.mk > config.mk.tmp
	@mv config.mk.tmp config.mk

