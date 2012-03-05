BENCHMARK_NAME = sva
SOURCES = sva.c 

include ../../Makefile.common

test: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	./mt_prog

dtest: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	./mt_prog
