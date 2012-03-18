#ifndef __HYBRID_DIRECTORY_H__
#define __HYBRID_DIRECTORY_H__

#include "sim.h"
#include "overflow_directory.h"

// Each LRB entry is a cache-line in size
const uint32_t HYBRIDCC_COARSE_TABLE_ENTRIES = 3;

struct coarse_table_entry_t {
  uint32_t start_addr;
  uint32_t end_addr;
  bool valid;
  bool incoherent_malloc_region;

  // Initialize the entry.
  void set(uint32_t addr_start, uint32_t addr_end, bool ic_malloc) {
    start_addr = addr_start;
    end_addr = addr_end;
    valid = true;
    incoherent_malloc_region = ic_malloc;
  }

  // Return true if address is in region.
  bool addr_match(uint32_t addr) const {
    using namespace rigel;
    if (incoherent_malloc_region && !CMDLINE_ENABLE_INCOHERENT_MALLOC) { return false; }
    if (!valid) return false;
    return ((addr >= start_addr) &&( addr < end_addr));
  }
  // Reset the entry.
  void reset() { valid = false; start_addr = 0; end_addr = 0; incoherent_malloc_region = false; }
};

class HybridDirectory
{
  public: /* ACCESSORS */
    // Constructor initializes tables.
    HybridDirectory();
    // Return true if region is in coarse-grain region table.
    bool check_coarse_region_table_hit(uint32_t addr) const;
    // Returns true if the region should not be tracked by coherence.  Only true
    // when hybrid coherence is turned on.
    bool hybrid_coherence_enabled(uint32_t addr, directory::dir_of_timing_t &dir_timer);
    // Update the fine-grain region table if the address fals in the region
    // table's address range.  Called from G$ model on a write.
    void update_region_table(uint32_t addr, uint32_t data);
    // Initialize the hybrid directory.
    void init();
    // Set the base address of the region table.  Called from SPRF.write()
    void set_fine_region_table_base(uint32_t addr) { 
      fine_region_table_base_addr = addr;
//  uint32_t fine_region_table_end_addr 
//    = fine_region_table_base_addr+(fine_region_table_size_words * 4);
//DEBUG_HEADER(); 
//fprintf(stderr, "HYBRIDCC: [REGION SET BASE] fine_region_table_base_addr: %08x end: %08x\n", 
//addr, fine_region_table_end_addr);
}
    // Return the word offset into the fine-grain region table.
    static uint32_t calc_frt_update_table_offset(uint32_t addr);
    static uint32_t calc_frt_access_table_offset(uint32_t addr);
    // Return the bit index of the address.
    static uint32_t calc_frt_update_bit_idx(uint32_t addr);
    static uint32_t calc_frt_access_bit_idx(uint32_t addr);
  private: /* ACCESSORS */
    // Returns true if it is a fine-graine region table access.
    bool frt_access(uint32_t addr);

  public: /* DATA */
    // Base address in target memory for the fine-grain region table.
    static uint32_t fine_region_table_base_addr;
    static uint32_t fine_region_table_size_words;
    // Table in memory holding the coherence states.
    static uint32_t *fine_region_table;

  private: /* DATA */
    // Coarse regions to track as incoherent, e.g., global, code, stacks
    coarse_table_entry_t coarse_region_table[HYBRIDCC_COARSE_TABLE_ENTRIES];
};

// Global instance of the hybrid directory.
extern HybridDirectory hybrid_directory;

inline bool 
HybridDirectory::frt_access(uint32_t addr)
{
  uint32_t fine_region_table_end_addr 
    = fine_region_table_base_addr+(fine_region_table_size_words * 4);
  if (addr >= fine_region_table_base_addr && addr < fine_region_table_end_addr) { return true; }

  return false;
}
#endif
