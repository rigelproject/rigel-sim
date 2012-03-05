SUBDIRS=simple trivial
MAKE=make

test:
	@for i in $(SUBDIRS); do \
	echo "[[ -> CWD test: $$i ]]"; \
	(cd $$i; $(MAKE) -s test); done

clean:
	@for i in $(SUBDIRS); do \
	echo "[[ -> CWD clean: $$i ]]"; \
	(cd $$i; $(MAKE) -s clean); done
