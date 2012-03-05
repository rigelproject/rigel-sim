BENCHMARK_NAME = cg
SOURCES = cg.c cg-util.c

include ../../Makefile.common

test: clean ${BENCHMARK_NAME}.tasks
	./run_prog

dtest: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	./mt_prog
