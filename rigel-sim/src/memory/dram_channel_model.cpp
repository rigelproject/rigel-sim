////////////////////////////////////////////////////////////////////////////////
//  dram_channel_model.cpp
////////////////////////////////////////////////////////////////////////////////
//
//  Cycle-accurate model of DDR DRAM.  It serves mostly as a checker.  The
//  memory model is what generates requests and gets replies.  The DRAMChannelModel
//  just checks that the actions taken by the memory controller abide by the
//  constraints of the DRAM parameters listed in sim.h, e.g., CAS
//  delay, RAS delay, etc.
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <inttypes.h>                   // for PRIu64, PRIu32
#include <stdint.h>                     // for uint32_t, uint64_t
#include <stdio.h>                      // for printf, NULL
#include <set>                          // for set
#include "memory/address_mapping.h"  // for AddressMapping
#include "define.h"         // for DRAMState_t, etc
#include "memory/dram.h"           // for BANKS, CMD_WINDOW_SIZE, etc
#include "memory/dram_channel_model.h"  // for DRAMChannelModel, etc
#include "profile/profile.h"        // for ProfileStat, MemStat, etc
#include "profile/profile_names.h"
#include "sim.h"            // for stats, etc

int DRAMChannelModel::MaxDelay;
uint64_t DRAMChannelModel::DRAM_CURR_CYCLE;
unsigned int rigel::DRAM::ROWS;
unsigned int rigel::DRAM::RANKS;
unsigned int rigel::DRAM::CONTROLLERS;
unsigned int rigel::DRAM::OPEN_ROWS;

/*
#define DEBUG
#define DRAM_DEBUG
#define DEBUG_PRINT_DRAM_TRACE
*/

////////////////////////////////////////////////////////////////////////////////
// DRAMChannelModel Constructor
////////////////////////////////////////////////////////////////////////////////
DRAMChannelModel::DRAMChannelModel(int id) : channelID(id) {
  using namespace rigel;

  CurrCmd = 0;
  MaxDelay = -1;
  BurstCnt = 0;
    DataArray = new DRAMRank *[DRAM::RANKS];
  for (unsigned int i = 0; i < DRAM::RANKS; i++) {
    DataArray[i] = new DRAMRank;
  }

  ActiveRow = new ActiveRowEntry *[DRAM::RANKS];
  CurrState = new DRAMState_t *[DRAM::RANKS];
  NextState = new DRAMState_t *[DRAM::RANKS];
  PrechargePending = new std::set<uint32_t> *[DRAM::RANKS];
  RowPendingActivate = new std::set<uint32_t> *[DRAM::RANKS];

  for (unsigned int i = 0; i < DRAM::RANKS; i++) {
    ActiveRow[i] = new ActiveRowEntry [DRAM::BANKS];
    CurrState[i] = new DRAMState_t [DRAM::BANKS];
    NextState[i] = new DRAMState_t [DRAM::BANKS];
    PrechargePending[i] = new std::set<uint32_t> [DRAM::BANKS];
    RowPendingActivate[i] = new std::set<uint32_t> [DRAM::BANKS];
  }

  HeadCmd = 0;

  for (unsigned int bank = 0; bank < DRAM::BANKS; bank++) {
    for (unsigned int rank = 0; rank < DRAM::RANKS; rank++) {
      ActiveRow[rank][bank].ptr = NULL;
      ActiveRow[rank][bank].idx = -1;
      CurrState[rank][bank] = DRAM_IDLE;
      NextState[rank][bank] = DRAM_IDLE;
    }
  }

  for (int i = 0; i < DRAM::CMD_WINDOW_SIZE; i++) {
    DataBusBusy[i] = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
// DRAMChannelModel Destructor
////////////////////////////////////////////////////////////////////////////////
DRAMChannelModel::~DRAMChannelModel() {
  using namespace rigel;
  for (unsigned int i = 0; i < DRAM::RANKS; i++) {
    if (DataArray[i]) delete DataArray[i];
    DataArray[i] = NULL;
  }
}


////////////////////////////////////////////////////////////////////////////////
// DRAMChannelModel::NextCmdWindowEntryOffset()
////////////////////////////////////////////////////////////////////////////////
int
DRAMChannelModel::NextCmdWindowEntryOffset()
{
  using namespace rigel::DRAM;

#ifdef DRAM_DEBUG
  DEBUG_HEADER();
  cerr << "DRAMChannelModel::NextCmdWindowEntryOffset()"
    " HeadCmd: " << dec << HeadCmd <<
    " CurrCmd: " << dec << CurrCmd << endl;
#endif
  int delay = -1;

  if (CurrCmd < CMD_WINDOW_MAX_DELAY) {
    if ( (HeadCmd < CurrCmd) || (HeadCmd > CMD_WINDOW_SIZE - CurrCmd) ) {
      // The current bank is available any time after now
      delay = 1;
    } else {
      // There is an earlier command scheduled in front of CurrCmd
      delay = (HeadCmd - CurrCmd) + 1;
    }
  } else {
    if ( (HeadCmd < CurrCmd) && (HeadCmd > (CurrCmd - CMD_WINDOW_MAX_DELAY)) ) {
      delay = 1;
    } else {
      if (HeadCmd < CurrCmd) {
        delay = (HeadCmd - CurrCmd + CMD_WINDOW_SIZE) + 1;
      } else {
        delay = (HeadCmd - CurrCmd) + 1;
      }
    }
  }
  assert( (delay > 0) && "Delay should always be between 0 and CMD_WINDOW_MAX_DELAY");

  return delay;
}
// end NextCmdWindowEntryOffset

////////////////////////////////////////////////////////////////////////////////
// DRAMChannelModel::ReadData()
////////////////////////////////////////////////////////////////////////////////
// Read a single 32-bit word into 'data' from the memory model.
void
DRAMChannelModel::ReadData(uint32_t &data, uint32_t addr)
{
  data = DataArray[AddressMapping::GetRank(addr)]->
            Bank(AddressMapping::GetBank(addr))->
            Row(AddressMapping::GetRow(addr))->
            Col(AddressMapping::GetCol(addr));
  #ifdef DEBUG_DRAM
  DEBUG_HEADER();
  cerr << "DEBUG_DRAM: DRAMChannelModel::ReadData()";
  cerr << " ADDR: 0x" << HEX_COUT << addr;
  cerr << " DATA: 0x" << HEX_COUT << data;
  cerr << endl;
  #endif
}

////////////////////////////////////////////////////////////////////////////////
// DRAMChannelModel::WriteData()
////////////////////////////////////////////////////////////////////////////////
// Write a single 32-bit word (data) to the memory model at addr.
void
DRAMChannelModel::WriteData(uint32_t const &data, uint32_t addr)
{
  DataArray[AddressMapping::GetRank(addr)]->
            Bank(AddressMapping::GetBank(addr))->
            Row(AddressMapping::GetRow(addr))->
            Col(AddressMapping::GetCol(addr)) = data;
  #ifdef DEBUG_DRAM
  DEBUG_HEADER();
  cerr << "DEBUG_DRAM: DRAMChannelModel::WriteData()";
  cerr << " ADDR: 0x" << HEX_COUT << addr;
  cerr << " DATA: 0x" << HEX_COUT << data;
  cerr << endl;
  #endif
}

////////////////////////////////////////////////////////////////////////////////
// DRAMChannelModel::PerCycle()
////////////////////////////////////////////////////////////////////////////////
// Everthing is handled just in time for memory scheduling purposes.  The
// controller is responsible for putting valid entries into the CmdWindow
void
DRAMChannelModel::PerCycle()
{
  using namespace rigel;
  // Gate the DRAM model if the idealized model is in use.
  if (rigel::CMDLINE_ENABLE_IDEALIZED_DRAM) { return; }

  uint64_t CurrCycle = DRAMChannelModel::GetDRAMCycle();
  DataBusBusy[CurrCmd] = false;
  CmdWindowEntry &cmd = CmdWindow[CurrCmd];

  // Used by profiler to put DRAM access commands into the histogram
  using namespace rigel::profiler;
  uint64_t profiler_hist_bin = (rigel::CURR_CYCLE
              / gcacheops_histogram_bin_size)
              * gcacheops_histogram_bin_size;

  if(cmd.cmd != DRAM_CMD_INVALID && cmd.cmd != DRAM_CMD_NOP && cmd.valid)
  {
    uint32_t row = AddressMapping::GetRow(cmd.addr);
    unsigned int rank = cmd.rank;
    unsigned int bank = cmd.bank;
#ifdef DRAM_DEBUG
    DEBUG_HEADER();
    cerr << "((DRAM EXECUTE CurrCmd: " << dec << CurrCmd << ")) ";
#endif

    switch (cmd.cmd) {
      case (DRAM_CMD_ACTIVATE):
        ProfileStat::dram_hist_stats[profiler_hist_bin][PROF_DRAM_ACTIVATE]++;
        profiler::stats[STATNAME_DRAM_CMD_ACTIVATE].inc();

#ifdef DRAM_DEBUG
        cerr << " DRAM_CMD_ACTIVATE";
        cerr << " Row: " << dec <<  AddressMapping::GetRow(Addr);
        cerr << " Rank: " << dec <<  rank << " Bank: " << dec <<  bank << endl;
#endif
        if (LastAccess.ActiveBank[rank] != -1) {
          if (LastAccess.ActiveBank[rank] == (int)bank) {
            // Check RAS-to-RAS same bank
            assert((((uint32_t)(CurrCycle - LastAccess.RAS[rank][bank])) >= DRAM::LATENCY::RC)
              && "RAS-to-RAS (Same Bank) Delay Violated");
            // Check PRE-to-RAS same bank
            //printf("Rank %d, Bank %d, CURR_CYCLE %" PRIu64 ", Pre %" PRIu64 ", RP %" PRIu32 "\n",
            //    rank, bank, CURR_CYCLE,
            //   LastAccess.Precharge[rank][bank],
            //    DRAM::LATENCY::RP);
            assert(((CurrCycle - LastAccess.Precharge[rank][bank]) >=
              DRAM::LATENCY::RP) && "Precharge-to-RAS (Same Bank) Delay Violated");
          } else {
          // Check RAS-to-RAS diff bank
            for(unsigned int i = 0; i < DRAM::BANKS; i++)
              assert(((CurrCycle - LastAccess.RAS[rank][i]) >= DRAM::LATENCY::RRD)
                && "RAS-to-RAS (Diff. Bank) Delay Violated");
          }
        }
        //printf("Cycle %"PRIu64": Activate to rank %d bank %d row %d\n", CurrCycle, rank, bank, AddressMapping::GetRow(Addr));
        LastAccess.RAS[rank][bank] = CurrCycle;
        Profile::mem_stats.activate_bank[channelID][rank][bank]++;
        // Activate a new row
        ActiveRow[rank][bank].ptr = DataArray[rank]->Bank(bank)->Row(row);
        ActiveRow[rank][bank].idx = row;
        LastAccess.ActiveBank[rank] = bank;
        break;
      case (DRAM_CMD_PRECHARGE):
        ProfileStat::dram_hist_stats[profiler_hist_bin][PROF_DRAM_PRECHARGE]++;
        profiler::stats[STATNAME_DRAM_CMD_PRECHARGE].inc();
#ifdef DRAM_DEBUG
        cerr << " DRAM_CMD_PRECHARGE Row: ";
        cerr << dec << ActiveRow[rank][bank].idx;
        cerr << " Rank: " << dec << rank << " Bank: " << dec << bank << endl;
#endif
        LastAccess.Precharge[rank][bank] = CurrCycle;
        //printf("Cycle %"PRIu64": Precharge to rank %d bank %d\n", CurrCycle, rank, bank);

#ifdef DEBUG_PRINT_DRAM_TRACE
        DEBUG_HEADER();
        fprintf(stderr, "[[DRAM]] CMD: PRECHARGE ");
        fprintf(stderr, "row: %4d rank: %4d bank: %4d ", row, rank, bank);
        fprintf(stderr, "\n");
#endif
        // Explicitly precharge a bank
        // "Close" open row, NOP if none are open
        ActiveRow[rank][bank].ptr = NULL;
        ActiveRow[rank][bank].idx = -1;
        // For checking
        LastAccess.ActiveBank[rank] = -1;
        break;
      case (DRAM_CMD_READ):
        ProfileStat::dram_hist_stats[profiler_hist_bin][PROF_DRAM_READ]++;
        profiler::stats[STATNAME_DRAM_CMD_READ].inc();
#ifdef DRAM_DEBUG
        cerr << " DRAM_CMD_READ";
        fprintf(stderr, " 0x%08x", Addr);
        cerr << " Row (Open): " << dec <<  ActiveRow[rank][bank].idx;
        cerr << " Row (Requesting): " << dec <<  row;
        cerr << " Rank: " <<  dec << rank << " Bank: " << dec <<  bank << endl;
#endif
        LastAccess.CASread[rank][bank] = CurrCycle;

#ifdef DRAM_DEBUG
        printf("CURR_CYCLE %llu RAS[%d][%d] %llu RCDR %d\n",CURR_CYCLE,rank,bank,LastAccess.RAS[rank][bank],DRAM::LATENCY::RCDR);
#endif
        //printf("Cycle %"PRIu64": Read to rank %d bank %d row %d\n", CurrCycle, rank, bank, ActiveRow[rank][bank].idx);
        // Check RCDR.  Only check if this is not currently the active row.
        if((CurrCycle - LastAccess.RAS[rank][bank]) < DRAM::LATENCY::RCDR)
        {
        printf("Cycle %"PRIu64": Error: Rank %u, Bank %u, tRCDR = %"PRIu32" cycles, only waited %"PRIu64" cycles\n", CurrCycle, rank, bank, DRAM::LATENCY::RCDR, (CurrCycle - LastAccess.RAS[rank][bank]));
        }
        assert(((CurrCycle - LastAccess.RAS[rank][bank]) >= DRAM::LATENCY::RCDR) && "RAS-to-CAS (Rd) Delay Violated");
        break;
      case (DRAM_CMD_WRITE):
        ProfileStat::dram_hist_stats[profiler_hist_bin][PROF_DRAM_WRITE]++;
        profiler::stats[STATNAME_DRAM_CMD_WRITE].inc();
#ifdef DRAM_DEBUG
        cerr << " DRAM_CMD_WRITE";
        fprintf(stderr, " 0x%08x", Addr);
        cerr << " Row (Open): " << dec <<  ActiveRow[rank][bank].idx;
        cerr << " Row (Requesting): " << dec <<  row;
        cerr << " Rank: " <<  dec << rank << " Bank: " << dec <<  bank << endl;
#endif
        LastAccess.CASwrite[rank][bank] = CurrCycle;

#ifdef DRAM_DEBUG
        printf("CURR_CYCLE %llu RAS[%d][%d] %llu RCDW %d\n",CURR_CYCLE,rank,bank,LastAccess.RAS[rank][bank],DRAM::LATENCY::RCDW);
#endif
        //printf("Cycle %"PRIu64": Write to rank %d bank %d row %d\n", CurrCycle, rank, bank, ActiveRow[rank][bank].idx);
        // Check RCDR.  Only check if this is not currently the active row.
        if((CurrCycle - LastAccess.RAS[rank][bank]) < DRAM::LATENCY::RCDW)
        {
          printf("Cycle %"PRIu64": Error: Rank %u, Bank %u, tRCDW = %"PRIu32" cycles, only waited %"PRIu64" cycles\n", CurrCycle, rank, bank, DRAM::LATENCY::RCDW, (CurrCycle - LastAccess.RAS[rank][bank]));
        }
        assert(((CurrCycle - LastAccess.RAS[rank][bank]) >= DRAM::LATENCY::RCDW) && "RAS-to-CAS (Wr) Delay Violated");
        break;
      case DRAM_CMD_NOP:
        ProfileStat::dram_hist_stats[profiler_hist_bin][PROF_DRAM_IDLE]++;
#ifdef DRAM_DEBUG
        cerr << " DRAM_CMD_NOP" << endl;
#endif
        profiler::stats[STATNAME_DRAM_CMD_IDLE].inc();
        break;
      default:
        assert(0 && "Unknown command!");
    }
    // Reset the command we just handled
    cmd.reset();
  }

  CmdWindowEntry &data = DataWindow[CurrCmd];

  if(data.cmd != DRAM_CMD_INVALID && data.cmd != DRAM_CMD_NOP && data.valid)
  {
    uint32_t ColAddr;
    uint32_t row = AddressMapping::GetRow(data.addr);
    unsigned int rank = data.rank;
    unsigned int bank = data.bank;
#ifdef DRAM_DEBUG
    DEBUG_HEADER();
    cerr << "((DRAM EXECUTE CurrCmd: " << dec << CurrCmd << ")) ";
#endif

    switch (data.cmd)
    {
      case (DRAM_CMD_READA):
        PrechargePending[rank][bank].insert(row);
      case (DRAM_CMD_READ):
      // Initiate a READ.
      {
        // Check CL
        //FIXME This check is not valid since you can pipeline CASes, so the LastAccess could be for a different read.
        //assert(CurrCycle - LastAccess.CASread[rank][bank] >= DRAM::LATENCY::CL && "CAS Latency Violated (CL)");
        // FIXME: Check Wr-to-Rd ?
#ifdef DEBUG_PRINT_DRAM_TRACE
        DEBUG_HEADER();
        fprintf(stderr, "[[DRAM]] CMD: READ     ");
        fprintf(stderr, "row: %4d rank: %4d bank: %4d ", row, rank, bank);
        fprintf(stderr, "addr: 0x%08x ", data.addr);
        fprintf(stderr, "\n");
#endif
        assert(AddressMapping::GetRank(data.addr) == (unsigned int)rank && AddressMapping::GetBank(data.addr) == (unsigned int)bank &&
               "Address/Rank-Bank Mismatch! Check for invalid cmd or SendCommand");
        // Continue READ
#ifdef DRAM_DEBUG
				ColAddr = AddressMapping::GetCol(BurstMask(data.addr)+sizeof(uint32_t)*BurstCnt);
        cerr << "DRAM_CMD_READ";
        cerr << " ColAddr: " << dec << ColAddr;
        cerr << " addr: 0x" << HEX_COUT << data.addr;
        cerr << " Row (Open): " << dec << ActiveRow[rank][bank].idx;
        cerr << " Row (Requesting): " <<dec << AddressMapping::GetRow(data.addr);
        cerr << " Rank: " << dec << rank << " Bank: " << Bank << endl;
#endif
        //FIXME Commenting out below lines because you actually can precharge a bank before the data is back.
        //We can fix this by assigning the row pointer to the data window entry when the READ command issues,
        //or maintaining a history of what the open row was when the READ command was issued.
        /*
        if(ActiveRow[rank][bank].ptr == NULL)
        {
          cerr << "Error: ActiveRow[" << rank << "][" << bank << "].ptr is NULL for address 0x" << std::hex << data.addr << endl;
          assert(0);
        }
        data.data_a = ActiveRow[rank][bank].ptr->Col(ColAddr);
        */
        BurstCnt++;
        // PROFILE: Histogram accessess based on bank
        Profile::mem_stats.read_bank[channelID][rank][bank]++;
        //See above FIXME
/*
        ColAddr = AddressMapping::GetCol(BurstMask(data.addr)+sizeof(uint32_t)*BurstCnt);
        data.data_b = ActiveRow[rank][bank].ptr->Col(ColAddr);
*/
        BurstCnt++;
        Profile::mem_stats.read_bank[channelID][rank][bank]++;

        // READ Done
        if (BurstCnt == DRAM::BURST_SIZE) {
          // TODO: Check for auto precharge
          BurstCnt = 0;
        }

        break;
      }
      case (DRAM_CMD_WRITEA):
        PrechargePending[rank][bank].insert(row);
      case (DRAM_CMD_WRITE):
        // Initiate a WRITE.  Check for autoprecharge
      {
#ifdef DEBUG_PRINT_DRAM_TRACE
        DEBUG_HEADER();
        fprintf(stderr, "[[DRAM]] CMD: WRITE    ");
        fprintf(stderr, "row: %4d rank: %4d bank: %4d ", row, rank, bank);
        fprintf(stderr, "addr: 0x%08x ", data.addr);
        fprintf(stderr, "\n");
#endif
        // Check WL
        //FIXME This check is not valid since you can pipeline CASes, so the LastAccess could be for a different write.
        //assert(CurrCycle - LastAccess.CASwrite[rank][bank] >= DRAM::LATENCY::WL && "Write Latency Violated (WL)");
        // Continue WRITE
#ifdef DRAM_DEBUG
				ColAddr = AddressMapping::GetCol(BurstMask(data.addr)+sizeof(uint32_t)*BurstCnt);
        cerr << "DRAM_CMD_WRITE";
        cerr << " ColAddr: " << dec << ColAddr;
        cerr << " addr: 0x" << HEX_COUT << data.addr;
        cerr << " Row (Open): " << dec <<  ActiveRow[rank][bank].idx;
        cerr << " Row (Requesting): " << dec <<  AddressMapping::GetRow(data.addr);
        cerr << " Rank: " << dec <<  rank << " Bank: " << dec <<  bank << endl;
#endif
        BurstCnt++;
        Profile::mem_stats.write_bank[channelID][rank][bank]++;
        ColAddr = AddressMapping::GetCol(BurstMask(data.addr)+sizeof(uint32_t)*BurstCnt);
        BurstCnt++;
        Profile::mem_stats.write_bank[channelID][rank][bank]++;
        // WRITE done
        if (BurstCnt == DRAM::BURST_SIZE) {
          // TODO: Check for auto precharge
          BurstCnt = 0;
        }
        break;
      }
      default:
        assert(0 && "Unknown command!");
    }
    // Reset the command we just handled
    data.reset();
  }
  CurrCmd = (CurrCmd + 1) % DRAM::CMD_WINDOW_SIZE;
}

///////////////////////////////////////////////////////////
// DRAMChannelModel::ScheduleCommand()
////////////////////////////////////////////////////////////////////////////////
// TODO: what does this do? what is a command? where are they defined? where is
// this used?
void
DRAMChannelModel::ScheduleCommand(uint32_t addr, DRAMCommand_t cmd, int delay)
{
  int rank = AddressMapping::GetRank(addr);
  int bank = AddressMapping::GetBank(addr);
  int CmdIdx = GetCmdIdxFromDelay(delay);
  CmdWindowEntry &CmdEntry = CmdWindow[CmdIdx];
  if(CmdEntry.valid)
    assert(0 && "Error: CmdWindow entry already valid\n");
  CmdEntry.valid = true;
  CmdEntry.cmd = cmd;
  CmdEntry.rank = rank;
  CmdEntry.bank = bank;
  CmdEntry.addr = addr;
} // end ScheduleCommand()

////////////////////////////////////////////////////////////////////////////////
// DRAMChannelModel::ScheduleData()
////////////////////////////////////////////////////////////////////////////////
// TODO: what does this do? what is a command? where are they defined? where is
// this used?
void
DRAMChannelModel::ScheduleData(uint32_t addr, bool ReadNOTWrite, int delay)
{
  int rank = AddressMapping::GetRank(addr);
  int bank = AddressMapping::GetBank(addr);
#ifdef DEBUG_PRINT_DRAM_TRACE
  DEBUG_HEADER();
  fprintf(stderr, "[[DRAM]] Cycle %"PRIu64" ScheduleData ", DRAMChannelModel::GetDRAMCycle());
  fprintf(stderr, "delay: %d CurrCmd 0x%02x CmdIdx for Delay 0x%02x",
    delay, CurrCmd, GetCmdIdxFromDelay(delay));
  fprintf(stderr, "\n");
#endif
  profiler::stats[STATNAME_DRAM_BUS_BUSY_CYCLES].inc();
  //TODO: Now using DRAMCommand_t under the hood for convenience, should break out into a separate enum maybe.
  DRAMCommand_t cmd = ReadNOTWrite ? DRAM_CMD_READ : DRAM_CMD_WRITE;
  int CmdIdx = GetCmdIdxFromDelay(delay);
  CmdWindowEntry &CmdEntry = DataWindow[CmdIdx];
  if(CmdEntry.valid)
    assert(0 && "Error: DataWindow entry already valid\n");
  CmdEntry.valid = true;
  CmdEntry.cmd = cmd;
  CmdEntry.rank = rank;
  CmdEntry.bank = bank;
  CmdEntry.addr = addr;
} // end ScheduleData()

#undef DEBUG
#undef DRAM_DEBUG
#define DEBUG_PRINT_DRAM_TRACE
