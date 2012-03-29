////////////////////////////////////////////////////////////////////////////////
// dram_channel_model.h
////////////////////////////////////////////////////////////////////////////////
//
//  (G)DDR DRAM checker model definition.  The class is used to check the requests
//  of the memory controller against timing parameters
//  definined in dram.h.  A single instance of DRAMChannelModel models a single channel
//  of DRAM, comprised of multiple ranks, multiple chips/rank. banks/chip, rows/bank,
//  and columns/row.
////////////////////////////////////////////////////////////////////////////////

#ifndef __DRAM_CHANNEL_MODEL_H__
#define __DRAM_CHANNEL_MODEL_H__

#include "sim.h"
#include "memory/address_mapping.h"
#include <iostream>
#include <set>

using namespace rigel;

/// We allocate target memory as needed on a DRAM page basis.  This yields something
/// like 2 words of overhead per page, or <1%.
struct DRAMRow {
  ///We initialize each word of target memory to rigel::memory::MEMORY_INIT_VALUE.
  DRAMRow() { for(unsigned int i = 0; i < DRAM::COLS; i++) { c[i] = rigel::memory::MEMORY_INIT_VALUE; } }

  uint32_t &Col(unsigned int i) { 
    if(!(i < DRAM::COLS))
    {
			std::cerr << "Error: Trying to access DRAM column " << i << " (only " << DRAM::COLS << " columns per DRAM row)\n";
      assert(0);
    }
    #ifdef DEBUG_DRAM
    std::cerr << "DEBUG_DRAM: DRAMRow::Col() Accessing: " << dec << i << "\n";
    #endif
    return c[i]; 
  }
  uint32_t c[DRAM::COLS]; 
};
struct DRAMBank { 
  /// Fault in a new row if needed
  DRAMRow *Row(unsigned int i) {
    assert(i < DRAM::ROWS);
    #ifdef DEBUG_DRAM
    std::cerr << "DEBUG_DRAM: DRAMBank::Row() Accessing: " << dec << i << "\n";
    #endif
    if (NULL != r[i]) return r[i];
    r[i] = new DRAMRow();
    #ifdef DEBUG_DRAM
    std::cerr << "DEBUG_DRAM: DRAMBank::Row() Creating: " << dec << i << "\n";
    for (int j = 0; j < DRAM::COLS; j++) {  r[i]->Col(j) = 0; }
    #endif
    return r[i];
  }
  // Return true if there is valid data in this row
  // r[i] == NULL --> invalid, r[i] != NULL --> valid
  bool RowValid(int i) {
    return (r[i] != NULL);
  }

  DRAMBank()
  {
    r = new DRAMRow *[DRAM::ROWS];
    for (unsigned int i = 0; i < DRAM::ROWS; i++)
      r[i] = NULL;
  }

  ~DRAMBank()
  {
    for (unsigned int i = 0; i < DRAM::ROWS; i++)
    { 
      if (r[i])
        delete r[i];
    }
    delete [] r;
  }

  DRAMRow **r;
};

struct DRAMRank { 
  DRAMBank *Bank(unsigned int i) { 
    #ifdef DEBUG_DRAM
    std::cerr << "DEBUG_DRAM: DRAMChip::Bank() Accessing: " << dec << i << "\n";
    #endif
    assert(i < DRAM::BANKS);
    return b[i];
  }

  DRAMRank() { for (unsigned int i = 0; i < DRAM::BANKS; i++) { b[i] = new DRAMBank(); } }
  ~DRAMRank() { for (unsigned int i = 0; i < DRAM::BANKS; i++) { delete b[i]; } }
  DRAMBank *b[DRAM::BANKS];
};

class DRAMChannelModel {
  public:
    ///We maintain a window of commands for each DRAM bus cycle.
    ///The size of this window is bounded by the maximum latency of
    ///any command.
    typedef struct _CmdWindowEntry{
      _CmdWindowEntry() {
        reset();
      }

      DRAMCommand_t cmd;
      uint32_t data_a;
      uint32_t data_b;
      uint32_t addr;
      int rank;
      int bank;
      // TODO: Add write masking
      uint8_t  write_mask;
      bool valid;

      void reset() {
        cmd = DRAM_CMD_INVALID;
        valid = false;
        data_a = 0xadddddda;
        data_b = 0xbddddddb;
        rank = -1;
        bank = -1;
        addr = 0xdeadbeef;
        write_mask = 0;
      }

    } CmdWindowEntry;
  private:
    static int MaxDelay;

  public:
    DRAMChannelModel(int id);
    ~DRAMChannelModel();

    static int GetMaxDelay() { return MaxDelay; }

    // Get the current DRAM cycle number (usually >= CPU Clk)
    static uint64_t GetDRAMCycle() {
      return DRAM_CURR_CYCLE;
    }
    // Analagous to CURR_CYCLE but for DRAM.
    static uint64_t DRAM_CURR_CYCLE;

    int GetCmdIdxFromDelay(int delay) {
      // Keep track of maximum delay seen by memory controller.
      if (delay > MaxDelay) MaxDelay = delay;

      return (CurrCmd+delay) % DRAM::CMD_WINDOW_SIZE;
    }

    // Basic command bus
    void ScheduleCommand(uint32_t addr, DRAMCommand_t cmd, int delay);
    void ScheduleData(uint32_t addr, bool ReadNOTWrite, int delay);

    // Bidirectional data bus.  A is first half cycle, B is second
    void DataBusA(int rank, int bank, uint32_t &data, int delay) {
      assert(0 && "Right now we do just-in-time DRAM access...FIXME?");
      DataWindow[GetCmdIdxFromDelay(delay)].data_a = data;
     }
    void DataBusB(int rank, int bank, uint32_t &data, int delay) {
      assert(0 && "Right now we do just-in-time DRAM access...FIXME?");
      DataWindow[GetCmdIdxFromDelay(delay)].data_b = data;
    }

    // Write mask. Most Sig. Nibble is A, LSN is B
    void WriteMask(int rank, int bank, uint8_t mask, int delay) {
      CmdWindow[GetCmdIdxFromDelay(delay)].write_mask = mask;
      DataWindow[GetCmdIdxFromDelay(delay)].write_mask = mask;
    }

    // Do one memory cycle
    void PerCycle();

    // Instant back door access
    void ReadData(uint32_t &data, uint32_t addr);
    void WriteData(uint32_t const &data, uint32_t addr);

    // Check to see if row has been generated yet
    bool IsValidAddr(uint32_t addr) {
        return (DataArray[AddressMapping::GetRank(addr)]->
                Bank(AddressMapping::GetBank(addr))->
                RowValid(AddressMapping::GetRow(addr)));
    }

    // Return true if the active row (in its respective bank) is open for the
    // provided address
    int RowActiveIdx(int rank, int bank) {
      // All rows in all chips should be open, so this is safe.
      return (ActiveRow[rank][bank].idx);
    }

    // Data array
    DRAMRank ** DataArray;
    // Pointer to the current command being evaluated
    int CurrCmd;
    // The furthest into future a command has been scheduled
    int HeadCmd;
    // Holds all of the commands
    CmdWindowEntry CmdWindow[rigel::DRAM::CMD_WINDOW_SIZE];
    // Holds data transfer commands
    //FIXME kind of a hack
    CmdWindowEntry DataWindow[rigel::DRAM::CMD_WINDOW_SIZE];
    // Is the data bus busy in a given cycle?
    bool DataBusBusy[DRAM::CMD_WINDOW_SIZE];
    void SetDataBusBusy(int delay);
    bool CheckDataBusBusy(int delay) { return DataBusBusy[GetCmdIdxFromDelay(delay)]; }
    bool CheckDataBusBusy(int delay, int count) {
#ifdef DEBUG_PRINT_DRAM_TRACE
    DEBUG_HEADER();
    fprintf(stderr, "[[DRAM]] CheckDataBusBusy ");
    fprintf(stderr, "delay: %d CurrCmd 0x%02x CmdIdx for Delay 0x%02x count: %d",
      delay, CurrCmd, GetCmdIdxFromDelay(delay), count);
    fprintf(stderr, "\n");
#endif
      for (int i = 0; i < count; i++) {
        if (CheckDataBusBusy(delay+i)) return true;
      }
      // Not busy
      return false;
    }



    // Find the next command that is currently filled with a NOP where we can
    // schedule something
    int NextCmdWindowEntryOffset();


    // Values modified by the FSM for each bank
    DRAMState_t ** CurrState, ** NextState;
    // Each bank has a row active or it is IDLE and ActiveRow == NULL
    typedef struct { DRAMRow *ptr; int idx; } ActiveRowEntry;

    ActiveRowEntry ** ActiveRow;
    uint32_t ActiveAddr;
    // Bursts are fixed and uninterruptable so only one counter is needed.
    int BurstCnt;
    // Set by an auto precharge READ/WRITE command.  Evaluated at end of burst
    std::set<uint32_t> ** PrechargePending;
  private:
    //Each bank has a set of row numbers that are in line to be activated.
    // Updated when a Row activate is scheduled
    std::set<uint32_t> ** RowPendingActivate;
    //Which DRAM channel am I?
    int channelID;
  public:
    uint32_t BurstMask(uint32_t addr) {
      using namespace rigel::DRAM;
      uint32_t mask = ~((1 << (rigel_log2(COLSIZE) + rigel_log2(BURST_SIZE))) - 1);
      return addr & mask;
    }

    bool isRowPendingActivate(uint32_t rank, uint32_t bank, uint32_t row)
    {
      return RowPendingActivate[rank][bank].count(row) != 0;
    }

    // Debug Data For Ensuring DRAM timings are respected
    struct _LastAccess {

      _LastAccess() {
        Precharge = new uint64_t *[DRAM::RANKS];
        RAS = new uint64_t *[DRAM::RANKS];
        CASread = new uint64_t *[DRAM::RANKS];
        CASwrite = new uint64_t *[DRAM::RANKS];
        ActiveBank = new int [DRAM::RANKS];

        for (unsigned int i = 0; i < DRAM::RANKS; i++) {
          Precharge[i] = new uint64_t [DRAM::BANKS];
          RAS[i] = new uint64_t [DRAM::BANKS];
          CASread[i] = new uint64_t [DRAM::BANKS];
          CASwrite[i] = new uint64_t [DRAM::BANKS];

          for (unsigned int j = 0; j < DRAM::BANKS; j++) {
            Precharge[i][j] = 0;
            RAS[i][j] = 0;
            CASread[i][j] = 0;
            CASwrite[i][j] = 0;
          }
          ActiveBank[i] = -1;
        }
      }
      uint64_t **Precharge;
      uint64_t **RAS;
      uint64_t **CASread;
      uint64_t **CASwrite;
      int *ActiveBank;
    } LastAccess;
};
#endif
