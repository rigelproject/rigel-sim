
#include "util/rigelprint.h"
#include "core/regfile.h" // for regval32_t, relocate to remove rf dependence
#include <cstdio>
#include <inttypes.h>

namespace rigel {
  void RigelPRINTREG(uint32_t pc, regval32_t regval, int coreid, int tid) {
   // Enable short RigelPrint() printing for faster consumption by scripts.
    if (rigel::CMDLINE_SHORT_RIGELPRINT) {
      // CYCLE, PC, DATA.u32, CORE
      fprintf(rigel::RIGEL_PRINT_FILEP, "0x%012llx, 0x%08x, 0x%08x, %d\n",
        (unsigned long long)rigel::CURR_CYCLE,
        pc,
        regval.u32(),
        coreid);
    } else {
      fprintf(rigel::RIGEL_PRINT_FILEP,
        //"0x%012llx (PC 0x%08x) PRINTREG:  0x%08x (%f f) core: %d thread: %d\n",
        "0x%012llx (PC 0x%08x) PRINTREG: 0x%08x %09"PRIu32" %09"PRId32" (%f f) core: %d thread: %d\n",
        (unsigned long long)rigel::CURR_CYCLE,
        pc,
        regval.u32(), 
        regval.u32(), 
        regval.i32(), 
        regval.f32(),
        coreid,
        tid);
    }
  }; 

}
