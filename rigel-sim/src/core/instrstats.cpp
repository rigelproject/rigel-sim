
 // InstrStats defininitions
 // Return type information based on decoded type, not current type in instr
#include "instr.h"          // for InstrLegacy
#include "instrstats.h"     // for InstrStats

 bool InstrStats::is_local_store() { 
   return InstrLegacy::instr_is_local_store(decoded_instr_type); 
 }
 bool InstrStats::is_local_memaccess() { 
   return InstrLegacy::instr_is_local_memaccess(decoded_instr_type); 
 }
 bool InstrStats::is_global_atomic() { 
   return InstrLegacy::instr_is_global_atomic(decoded_instr_type); 
 }
 bool InstrStats::is_global()
 { 
   return InstrLegacy::instr_is_global(decoded_instr_type); 
 }
 bool InstrStats::is_atomic()
 { 
   return InstrLegacy::instr_is_atomic(decoded_instr_type); 
 }
 bool InstrStats::is_local_atomic()
 { 
   return InstrLegacy::instr_is_local_atomic(decoded_instr_type); 
 }
