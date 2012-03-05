# $Id: 6e91198fd94431af6c5d57e0dad27501a7d88b90 $

PROGRAMS = RandomExample RandomSave RandomThread \
	RandomCoverage RandomExact RandomLambda RandomTime

VSPROJECTS = $(addsuffix -vc8.vcproj,Random $(PROGRAMS)) \
	$(addsuffix -vc9.vcproj,Random $(PROGRAMS))

all:
	@:
install:
	@:
clean:
	@:
list:
	@echo $(VSPROJECTS) \
	RandomLib-vc8.sln RandomLib-vc9.sln

.PHONY: all install list clean
