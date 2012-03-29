AUTOMAKE_OPTIONS = foreign
#AM_LDFLAGS= -static -rdynamic -E -g 
AM_LDFLAGS= -rdynamic -g 
include $(top_srcdir)/Makefile.flags

# external sources, nothing by default
external_SOURCES =
external_INCLUDES =
include $(top_srcdir)/Makefile.external
INCLUDES += $(external_INCLUDES)

PROTOC=protoc

# Protobuffer code generation rules (note the first rule has multiple targets). 
%.pb.h %.pb.cc: %.proto
	$(PROTOC) -I`dirname $@` --cpp_out=`dirname $@` $< 

AM_CPPFLAGS=$(protobuf_CFLAGS)

bin_PROGRAMS = rigelsim dramtest
common_SOURCES = \
 src/autogen/simdecoder_new.cpp \
 src/isa/rigel_isa.cpp \
 src/tile/tile_legacy.cpp \
 src/tile/tile_new.cpp \
 src/tile/tile_base.cpp \
 src/profile/profile.cpp \
 src/profile/profile_init.cpp \
 src/interconnect/icmsg.cpp \
 src/interconnect/global_network.cpp \
 src/interconnect/global_network_ideal.cpp \
 src/interconnect/global_network_new.cpp \
 src/interconnect/tile_interconnect.cpp \
 src/interconnect/tile_interconnect_ideal.cpp \
 src/interconnect/tile_interconnect_new.cpp \
 src/interconnect/tile_interconnect_bcast.cpp \
 src/interconnect/tree_network.cpp \
 src/interconnect/crossbar.cpp \
 src/memory/memory_timing_ideal.cpp \
 src/memory/memory_timing_dram.cpp \
 src/memory/dram_controller.cpp \
 src/memory/dram_channel_model.cpp \
 src/memory/tlb.cpp \
 src/caches_legacy/broadcast_manager.cpp \
 src/caches_legacy/prefetch.cpp \
 src/caches_legacy/gcache_request_state_machine.cpp \
 src/caches_legacy/cache_model_read.cpp \
 src/caches_legacy/cache_model_write.cpp \
 src/caches_legacy/cache_model_instr.cpp \
 src/caches_legacy/cache_model_global.cpp \
 src/caches_legacy/cache_model.cpp \
 src/caches_legacy/core_caches.cpp \
 src/caches_legacy/cluster_cache.cpp \
 src/caches_legacy/global_cache.cpp \
 src/caches_legacy/directory.cpp \
 src/caches_legacy/directory_helpers.cpp \
 src/caches_legacy/overflow_directory.cpp \
 src/caches_legacy/hybrid_directory.cpp \
 src/caches_legacy/global_cache_coherence.cpp \
 src/caches_legacy/cluster_cache_coherence.cpp \
 src/caches_legacy/cluster_cache_util.cpp \
 src/stages/fp_complete.cpp \
 src/stages/ccache_access.cpp \
 src/stages/decode.cpp \
 src/stages/fetch.cpp \
 src/stages/writeback.cpp \
 src/stages/execute.cpp \
 src/stages/memory_req.cpp \
 src/stages/regfile_read.cpp \
 src/stages/bpredict.cpp \
 src/stages/stage_base.cpp \
 src/chip/chip.cpp \
 src/chip/chip_legacy.cpp \
 src/cluster/cluster_legacy.cpp \
 src/cluster/cluster_base.cpp \
 src/cluster/cluster_functional.cpp \
 src/cluster/cluster_structural.cpp \
 src/cluster/cluster_cache_base.cpp \
 src/cluster/cluster_cache_functional.cpp \
 src/cluster/cluster_cache_structural.cpp \
 src/core/corebase.cpp \
 src/core/core_functional.cpp \
 src/core/core_inorder_legacy.cpp \
 src/core/regfile.cpp \
 src/core/regfile_legacy.cpp \
 src/core/instrstats.cpp \
 src/core/btb.cpp \
 src/core/scoreboard.cpp \
 src/instr/instrbase.cpp \
 src/instr/instr_legacy.cpp \
 src/instr/pipe_packet.cpp \
 src/instr/static_decode_info.cpp \
 src/util/convert.cpp \
 src/util/rigel_strings.cpp \
 src/util/ui_legacy.cpp \
 src/util/debug.cpp \
 src/util/value_tracker.cpp \
 src/util/linenoise.c \
 src/util/event_track.cpp \
 src/util/elf_loader.cpp \
 src/util/cmdline_parse.cpp \
 src/util/syscalls.cpp \
 src/util/syscall_timers.cpp \
 src/util/task_queue.cpp \
 src/util/rigelprint.cpp \
 src/shell/shell.cpp \
 src/shell/commands.cpp \
 src/sim/component_base.cpp \
 src/protobuf/rigelsim.pb.cc \
 src/protobuf/rigelsim.pb.h

rigelsim_SOURCES = \
 $(common_SOURCES) \
 $(external_SOURCES) \
 src/sim.cpp

rigelsim_LDADD = $(LDADD) $(protobuf_LIBS)

dramtest_SOURCES = \
 $(common_SOURCES) \
 $(external_SOURCES) \
 src/dramtest/stream_generator.cpp \
 src/util/mem_trace/read_mem_trace_c.c \
 src/dramtest/dramtest.cpp

dramtest_LDADD = $(LDADD) $(protobuf_LIBS)

if ENABLE_COUCHDB
 rigelsim_SOURCES += src/couchdb/couchdb_simple.cpp
 dramtest_SOURCES += src/couchdb/couchdb_simple.cpp
endif
