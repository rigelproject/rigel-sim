# $Id: 18a7286cc6f252c94f8de4eaa630fd17a548cbbb $

MAKEFILE := $(lastword $(MAKEFILE_LIST))
MAKE := $(MAKE) -f $(MAKEFILE)
SUBDIRS = src examples doc
ALLDIRS = include $(SUBDIRS) windows cmake

all: src examples

$(SUBDIRS):
	$(MAKE) -C $@

examples: src
##install: install-headers install-lib install-examples install-cmake install-doc
install: install-headers install-lib install-examples install-cmake
clean: clean-src clean-examples clean-doc
distclean: clean distclean-doc
install-headers:
	$(MAKE) -C include install
install-lib:
	$(MAKE) -C src install
install-examples: src
	$(MAKE) -C examples install
install-cmake:
	$(MAKE) -C cmake install
install-doc: doc
	$(MAKE) -C doc install
clean-src:
	$(MAKE) -C src clean
clean-examples:
	$(MAKE) -C examples clean
clean-doc:
	$(MAKE) -C doc clean
distclean-doc:
	$(MAKE) -C doc distclean

list:
	@for f in 00README.txt COPYING.txt AUTHORS NEWS Makefile \
	$(MAKEFILE); do \
	  echo $$f; \
	done
	@for d in $(ALLDIRS); do \
	  (echo $(MAKEFILE); $(MAKE) -s -C $$d list) | tr ' ' '\n' | \
	  while read f; do echo $$d/$$f; done; \
	done

.PHONY: all $(SUBDIRS) install \
	install-headers install-lib install-examples install-cmake install-doc \
	clean clean-src clean-examples clean-doc list package
