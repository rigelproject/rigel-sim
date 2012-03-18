SUBDIRS= 
MAKE=make

test:
	@for i in $(SUBDIRS); do \
	echo "  [$$i]"; \
	(cd $$i; $(MAKE) -s test); done

dtest:
	echo "there is nothing here"

clean:
	@for i in $(SUBDIRS); do \
	echo "  [$$i]"; \
	(cd $$i; $(MAKE) -s clean); done

