#ifndef __GLOBAL_CACHE_DEBUG_H__
#define __GLOBAL_CACHE_DEBUG_H__
#if 0
#define __DEBUG_ADDR 0x02D22060
#ifdef __DEBUG_CYCLE
#undef __DEBUG_CYCLE
#endif
#define __DEBUG_CYCLE 0

#define __DEBUG_CC(addr, cluster, s) do {\
  DEBUG_HEADER(); \
  fprintf(stderr, "DEBUG_CC: (addr: 0x%08x cluster: %d sharers: ", \
    addr, cluster); \
  cache_directory->DEBUG_print_sharing_vector(addr, stderr); \
  fprintf(stderr, ") %s", s); \
} while (0);

#define DEBUG_CC(addr, cluster, s) do {\
  if ((addr & ~0x1F)  == (__DEBUG_ADDR & ~0x1F) && rigel::CURR_CYCLE > __DEBUG_CYCLE) {\
  __DEBUG_CC(addr, cluster, s);\
  fprintf(stderr, "\n");\
  }\
} while (0);

#define DEBUG_CC_MSG(addr, cluster, s, msg) do {\
  if ((addr & ~0x1F) == (__DEBUG_ADDR & ~0x1F) && rigel::CURR_CYCLE > __DEBUG_CYCLE) {\
  __DEBUG_CC(addr, cluster, s);\
  fprintf(stderr, " MSG TYPE: %3d\n", (int)(msg));\
  }\
} while (0);
#else
#define DEBUG_CC(addr, cluster, s)
#define DEBUG_CC_MSG(addr, cluster, s, msg)
#ifndef __DEBUG_ADDR
#define __DEBUG_ADDR 0xFFFFFFFF
#endif
#ifndef __DEBUG_CYCLE
#define __DEBUG_CYCLE 0xFFFFFFFF
#endif
#endif
#endif
