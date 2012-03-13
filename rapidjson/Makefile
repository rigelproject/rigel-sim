.PHONY: check-env

all:

check-env:
ifndef RIGEL_INSTALL
	$(error RIGEL_INSTALL environment variable is undefined)
endif

install: check-env
	cp -r include/rapidjson ${RIGEL_INSTALL}/host/include

#NOP
clean:

