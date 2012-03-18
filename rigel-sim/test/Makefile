SUBDIRS= non_rtm rtm ubench
# TODO: Add ASM tests
MAKE=make
SHELL=bash

test:
	pushd ../ && $(MAKE) -j 4 -s && popd
	@for i in $(SUBDIRS); do \
	echo "[$$i]"; \
	(cd $$i; $(MAKE) -s test); done

dtest:
	pushd ../ && $(MAKE) -j 4 -s && popd
	@for i in $(SUBDIRS); do \
	echo "[$$i]"; \
	(cd $$i; $(MAKE) -s dtest); done

clean:
	@for i in $(SUBDIRS); do \
	echo "[$$i]"; \
	(cd $$i; $(MAKE) -s clean); done

