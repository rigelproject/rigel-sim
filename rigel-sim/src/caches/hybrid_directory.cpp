////////////////////////////////////////////////////////////////////////////////
//  
////////////////////////////////////////////////////////////////////////////////
//

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t
#include "memory/address_mapping.h"  // for AddressMapping
#include "caches/hybrid_directory.h"  // for HybridDirectory, etc
#include "define.h"         // for ::IC_MSG_DIRECTORY_READ_REQ
#include "memory/dram.h"           // for CONTROLLERS
#include "overflow_directory.h"  // for dir_of_timing_entry_t, etc
#include "sim.h"            // for GCACHE_BANKS_PER_MC, etc

uint32_t HybridDirectory::fine_region_table_base_addr;
uint32_t HybridDirectory::fine_region_table_size_words;
uint32_t *HybridDirectory::fine_region_table;
using namespace rigel;

enum {
  COARSE_REGION_IDX_CODE = 0,
  COARSE_REGION_IDX_STACK= 1,
  COARSE_REGION_IDX_HEAP = 2,
};

HybridDirectory::HybridDirectory()
{
  using namespace rigel::cache;
  for (size_t i = 0; i < HYBRIDCC_COARSE_TABLE_ENTRIES; i++) {
    coarse_region_table[i].reset();
  }
  // Initialize the fine-grain region table.
  fine_region_table_size_words = (16*1024*1024 / sizeof(uint32_t));
  fine_region_table = new uint32_t [fine_region_table_size_words];
  for (size_t i = 0; i < fine_region_table_size_words; i++) {
    fine_region_table[i] = 0;
  }
  // The base addres should be set at startup by writing a SPR.
  fine_region_table_base_addr = 0;
}

void 
HybridDirectory::update_region_table(uint32_t addr, uint32_t data)
{
  // If hybrid coherence is not enabled, this is a nop.
  if (!CMDLINE_ENABLE_HYBRID_COHERENCE) return;
  // Check if the address is within the table's range.
  if (frt_access(addr))
  {
    // Convert the address into a word offset into the table.
    fine_region_table[calc_frt_update_table_offset(addr)] = data;
  }
}

bool 
HybridDirectory::hybrid_coherence_enabled(uint32_t addr, directory::dir_of_timing_t &dir_timer)
{
  using namespace rigel;
  using namespace rigel::cache;

  // Hybrid coherence not enabled.
  if (CMDLINE_ENABLE_HYBRID_COHERENCE)
  {
    // Check the code and stack regions first, they do not require going to
    // memory/L3 cache.
    if (coarse_region_table[COARSE_REGION_IDX_CODE].addr_match(addr)) { return true; }
    if (coarse_region_table[COARSE_REGION_IDX_STACK].addr_match(addr)) { return true; }

    // We are doing a look up so we have to insert an entry.
    uint32_t table_addr = fine_region_table_base_addr 
      + sizeof(uint32_t)*calc_frt_access_table_offset(addr);
    dir_timer.insert_target_addr(
      directory::dir_of_timing_entry_t(table_addr & LINE_MASK, IC_MSG_DIRECTORY_READ_REQ));

    // Check if the fine-grain table says it is coherent.
    if (fine_region_table[calc_frt_access_table_offset(addr)] 
        & (0x01UL << calc_frt_access_bit_idx(addr))) 
    {
      // If the bit is set, it must be coherent.
      return false;
    }

    if (coarse_region_table[COARSE_REGION_IDX_HEAP].addr_match(addr)) { return true; }
  }
  // All failed.
  return false;
}

// Global instance of the hybrid direectory.
HybridDirectory hybrid_directory;

uint32_t 
HybridDirectory::calc_frt_update_table_offset(uint32_t addr) 
{ 
  using namespace rigel::cache;
  uint32_t offset = ((addr - fine_region_table_base_addr) >> 2);
  return offset;
}

uint32_t 
HybridDirectory::calc_frt_access_table_offset(uint32_t addr) 
{ 
  using namespace rigel::cache;
  using namespace rigel::DRAM;
  
  // YY is 11 + log2(CTRLS)
  // [31 .. YY+10 ] [YY .. 11] [ YY+9 .. YY+1] [ 10 ]
  //uint32_t ctrl = GetController(addr);
  //uint32_t ctrl_shift = rigel_log2(COLS*COLSIZE);
  //uint32_t low_bit = (addr >> 10) & 0x1;
  //uint32_t mid_bits = (addr >> rigel_log2(COLS*COLSIZE)) & (; // addr >>
  //uint32_t high_bits = addr >> (rigel_log2(COLS*COLSIZE) + rigel_log2

  uint32_t gc_bank_num = (AddressMapping::GetController(addr)*GCACHE_BANKS_PER_MC) + 
              ((addr / (LINESIZE * GCACHE_CONTIGUOUS_LINES)) % GCACHE_BANKS_PER_MC);
  uint32_t bank_mask = (GCACHE_BANKS_PER_MC*CONTROLLERS-1) * (LINESIZE * GCACHE_CONTIGUOUS_LINES);

//  uint32_t mc_num = 

  uint32_t offset_addr = ((addr >> 8) & ~bank_mask) | 
                            (gc_bank_num * (LINESIZE*GCACHE_CONTIGUOUS_LINES));
  uint32_t offset = (offset_addr >> 2) & 0x003FFFFF;
  return offset;
}

uint32_t 
HybridDirectory::calc_frt_access_bit_idx(uint32_t addr) 
{ 
  using namespace rigel::cache;
  uint32_t bit_idx = (addr >> 5) & 0x1F; 
  return bit_idx;
}

void 
HybridDirectory::init()
{
  // Set up the code, stack, and incoherent malloc entries.
	// FIXME CODEPAGE_HIGHWATER_MARK doesn't exist anymore, the backing store tracks code regions
	// explicitly.  Once we expose that API to HybridDirectory we can do a better version of what
	// used to be below.  Use a dummy version of the commented out line below for now.
  //coarse_region_table[COARSE_REGION_IDX_CODE].set(0, CODEPAGE_HIGHWATER_MARK, false);
	coarse_region_table[COARSE_REGION_IDX_CODE].set(0, 0x10000, false);
  coarse_region_table[COARSE_REGION_IDX_STACK].set(0x7FFFFFFF, 0xFFFFFFFF, false);
  coarse_region_table[COARSE_REGION_IDX_HEAP].set(INCOHERENT_MALLOC_START, INCOHERENT_MALLOC_END, true);
}
