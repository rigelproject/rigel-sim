SUBDIRS= dmm sva cg cg-coherence1
DSUBDIRS=sva dmm cg
MAKE=make

test:
	@for i in $(SUBDIRS); do \
	echo "[[ -> CWD test: $$i ]]"; \
	(cd $$i; $(MAKE) -s test); done

dtest:
	@for i in $(DSUBDIRS); do \
	echo "[[ -> CWD test: $$i ]]"; \
	(cd $$i; $(MAKE) -s dtest); done

clean:
	@for i in $(SUBDIRS); do \
	echo "[[ -> CWD clean: $$i ]]"; \
	(cd $$i; $(MAKE) -s clean); done

