SUBDIRS=simple trivial
MAKE=make

test:
	@for i in $(SUBDIRS); do \
	echo "[$$i]"; \
	(cd $$i; $(MAKE) -s test); done

clean:
	@for i in $(SUBDIRS); do \
	echo "[$$i]"; \
	(cd $$i; $(MAKE) -s clean); done
