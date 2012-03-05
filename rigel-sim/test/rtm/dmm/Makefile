BENCHMARK_NAME = dmm
SOURCES = dmm.c 

include ../../Makefile.common

test: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	#./mt_prog

dtest: clean ${BENCHMARK_NAME}.tasks
	./run_prog
	./mt_prog
