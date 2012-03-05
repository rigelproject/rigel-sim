# $Id: 690983ffdbf2ab1f0c48b364eb08f82d5d4c3227 $

DEST = $(PREFIX)/share/cmake/RandomLib

INSTALL=install -b

all:
	@:
install:
	test -d $(DEST) || mkdir -p $(DEST)
	$(INSTALL) -m 644 FindRandomLib.cmake $(DEST)
list:
	@echo FindRandomLib.cmake \
	randomlib-config.cmake.in randomlib-config-version.cmake.in
clean:
	@:

.PHONY: all install list clean
