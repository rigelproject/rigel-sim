BENCHMARK_NAME = mvm
SOURCES = mvm.c

include ../../Makefile.common

test: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	./mt_prog

dtest: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	./mt_prog
