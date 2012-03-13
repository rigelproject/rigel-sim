#ifndef __DRAM_H__
#define __DRAM_H__

#include "sim.h" //For CLK_FREQ

namespace rigel {
  namespace DRAM {
    // Default scheduling policy for the memory controller.
    const char DRAM_SCHEDULING_POLICY_DEFAULT[] = "perbank";
    const char DRAM_BATCHING_POLICY_DEFAULT[] = "batch_none";
    const float CLK_FREQ = 6.0; //Data bus data rate, in Gbps
    //Note: DDR data, SDR command/address is assumed, so 6Gbps = 3 billion commands,
    //6 billion data bits per second

    // How much faster is the DRAM clock than the core clock (divide by 2.0 to account for DDR)
    // This determines how many times the DRAM controller and channel model will be clocked per
    // core clock.
    const float CLK_RATIO = rigel::DRAM::CLK_FREQ/2.0/rigel::CLK_FREQ;

    const unsigned int COLSIZE = 4; // 32 bits in a column
    const unsigned int COLS = 512;     // 2048 B/row
    extern unsigned int ROWS; //For non-power-of-2 CONTROLLERS, may have non-power-of-2 ROWS
    const unsigned int BANKS = 16;
    extern unsigned int RANKS;
    extern unsigned int CONTROLLERS;

    extern unsigned int OPEN_ROWS; //Number of open rows per bank (used to implement WC-DRAM)
    const char DRAM_OPEN_ROWS_DEFAULT[] = "1"; //This is a normal DRAM (1-entry cache)

    const int PHYSICALLY_ALLOWED_RANKS = 2;
    const bool ALWAYS_4GB = false;

    const int BURST_SIZE = 8;
    // Size of the scheduling window.  It is totally an artifact of my
    // implementation, but it must be larger than the longest possible delay.  A
    // power of two is always nice.  We make it twice as big to handle wrap
    // around  No command can be further in the future than CMD_WINDOW_MAX_DELAY
    const int CMD_WINDOW_MAX_DELAY = 256;
    const int CMD_WINDOW_SIZE = 2*CMD_WINDOW_MAX_DELAY;

    const int MAX_ACTIVATES_IN_WINDOW = 4;
    // Configuration for timing purposes.  Based on 6.0gbps GDDR5.
    namespace LATENCY {
      const uint32_t RAS = 45;    // ACT->PRE (minimum row active time)
      const uint32_t RC = 63;     // ACT->ACT in same bank
      const uint32_t RRD = 6;      // ACT->ACT in any bank (in the same rank)
      const uint32_t FAW = 24;     //No restriction from tFAW
      const uint32_t RCDR = 21; // RAS-to-CAS (ACT -> RD)
      const uint32_t RCDW = 12; // RAS-to-CAS (ACT -> WR)
      const uint32_t RP = 18;       // Cycles between PRE command and ACT command to same bank
      const uint32_t CL = 18;     // CAS Latency (cycles between READ command and first read data)
      const uint32_t WR = 21;     // Write recovery (cycles between last write data in and PRE command)
      const uint32_t WTR = 10;     // Number of cycles between end of write data burst and read command (caused by <2,4,8>n-prefetch)
      const uint32_t WL = 2;        // Write latency (cycles from WRITE command to write data)
                                    //Most GDDR5 parts will turn off input receivers for WL>3 (power savings), but we have the need for speed.
      const uint32_t RTP = 4;     //Number of cycles from READ command to PRECHARGE command
                                  //NOTE: This is generally less than CL, so you can PRECHARGE before your data comes back.
                                  //The memory controller and channel model should take this into account.
      const uint32_t CCD = 3;     //Number of cycles between consecutive CAS (READ or WRITE) commands. (tCCD)
                                  //NOTE: GDDR3-5 actually have tCCDL (long) and tCCDS (short) depending on whether
                                  //the accesses are to different banks/bank groups or the same bank/bank group.
                                  //Currently we don't model bank groups at all, so we use the more conservative value.
    };
  }
}

#endif //#ifndef __DRAM_H__

