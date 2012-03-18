SUBDIRS=trivial nosynch float fmadd malloc barrier simple_test coherence_test sva mvm syscalls
DSUBDIRS=trivial nosynch float fmadd malloc barrier simple_test sva mvm syscalls
MAKE=make

test:
	@for i in $(SUBDIRS); do \
	echo "[$$i]"; \
	(cd $$i; $(MAKE) -s test); done

dtest:
	@for i in $(DSUBDIRS); do \
	echo "[$$i]"; \
	(cd $$i; $(MAKE) -s dtest); done

clean:
	@for i in $(SUBDIRS); do \
	echo "[$$i]"; \
	(cd $$i; $(MAKE) -s clean); done

