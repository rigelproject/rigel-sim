BENCHMARK_NAME = simple_test
SOURCES = simple_test.c

include ../../Makefile.common

test: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	./mt_prog

dtest: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	./mt_prog
