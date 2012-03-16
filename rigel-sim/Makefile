BUILDDIR=${RIGEL_BUILD}/sim/rigel-sim
DEBUGDIR=${BUILDDIR}/debug
RELEASEDIR=${BUILDDIR}/release

.PHONY: docs

all: release
debug:
	$(MAKE) -C ${DEBUGDIR}
release:
	$(MAKE) -C ${RELEASEDIR}
clean: clean_debug clean_release
clean_debug:
	$(MAKE) -C ${DEBUGDIR} clean
clean_release:
	$(MAKE) -C ${RELEASEDIR} clean
install: install_release install_debug
install_debug:
	$(MAKE) -C ${DEBUGDIR} install
install_release:
	$(MAKE) -C ${RELEASEDIR} install

docs:
	cd $(RIGEL_SIM)/rigel-sim/docs; doxygen doxygen.cfg;
