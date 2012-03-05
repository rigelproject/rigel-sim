
////////////////////////////////////////////////////////////////////////////////
// bpredict.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file includes the branch predictor code for the pipeline model.  Both a
//  baseline (currently in use) and a gshare predictor are implemented.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __BPREDICT_H__
#define __BPREDICT_H__

#include "sim.h"
#include <string>

////////////////////////////////////////////////////////////////////////////////
// class BranchPredictorBase
//
// Base class for branch predictors.  All branch predictors derive from this
// class.
//
////////////////////////////////////////////////////////////////////////////////

class BranchPredictorBase {
  public:
    // Default constructor
    BranchPredictorBase(std::string _name) : need_fixup(false), name(_name) { 
      fprintf(stderr,"Branch Predictor may not work, code status unknown, you probably need to check that proper code paths exists to use this...\n");
      assert(0 && "Attempting to use deprecated branch predictor code!");
    }
    // Because it is virtual, we have to have a destructor.
    virtual ~BranchPredictorBase() { }

    virtual BranchPredictorBase &operator=(BranchPredictorBase &bp) {
      return *this;
    }

    bool need_fixup;    // Set on mispredict

    virtual void update(uint32_t currPC, uint32_t correctPC, bool was_bad_predict, 
                        bool taken, bool is_branch, uint32_t instr_gh) { }
    virtual uint32_t get_nPC(uint32_t currPC) { return 0xdeadbeef; }
    virtual uint32_t get_gHistory() {return 0xdeadbeef;}
    virtual uint32_t get_lHistory(uint32_t currPC) {return 0xdeadbeef;}
    virtual uint32_t peek_nPC(uint32_t currPC) {return 0xdeadbeef;}

		std::string name;
};

////////////////////////////////////////////////////////////////////////////////
// class BranchPredictorNextPC
////////////////////////////////////////////////////////////////////////////////
// Predicts all branches as not taken
class BranchPredictorNextPC : public BranchPredictorBase {
  public:
    BranchPredictorNextPC() : BranchPredictorBase("NextPC"), next_predPC(0) {
      fprintf(stderr,"Branch Predictor may not work, code status unknown, you probably need to check that proper code paths exists to use this...\n");
      assert(0 && "Attempting to use deprecated branch predictor code!");
    }

    ~BranchPredictorNextPC() { }

    inline uint32_t get_nPC(uint32_t currPC) { 
      uint32_t ret = next_predPC;
      /* Will be current PC + 4 unless update() has overridden */
      next_predPC += 4;
      return ret;
    }

    /* Allows for a check without updating the predictor */
    inline uint32_t peek_nPC(uint32_t currPC) {
      return next_predPC;
    }

    //Updates internal state
    //For this predictor, this function just overrides next_predPC when a branch is taken
    //(these will always be mispredicted)
    inline void update(uint32_t currPC, uint32_t correctPC, bool was_bad_predict, 
            bool taken, bool is_branch, uint32_t instr_gh) {
      //If last prediction was incorrect, override PC+4 with correct target
      if (was_bad_predict) { 
        next_predPC = correctPC;
      }
    }

    //Overloaded assignment operator (intelligently copies state)
    BranchPredictorNextPC &operator=(BranchPredictorNextPC &bpb)
    {
      next_predPC = bpb.next_predPC;
      return *this;
    }

  private:
    uint32_t next_predPC;
};

////////////////////////////////////////////////////////////////////////////////
// struct GShareTableEntry
////////////////////////////////////////////////////////////////////////////////
// The data held for each entry of the GShare predictor table.
typedef struct GShareTableEntry {
  uint32_t branch_addr;
  uint32_t target_addr;
  uint32_t counter;
  bool valid;
} GShareTableEntry;

////////////////////////////////////////////////////////////////////////////////
// class BranchPredictorGShare
////////////////////////////////////////////////////////////////////////////////
// Simple GShare BTB
class BranchPredictorGShare : public BranchPredictorBase {
  public:
    BranchPredictorGShare() :
      BranchPredictorBase("GShare"), next_predPC(0), fixupPC(0xdeadbeef)
    {
      //Initialize GShare table
      for (uint32_t i = 0; i < rigel::predictor::GSHARE_SIZE; i++) {
        target_buffer[i].valid = false;
        target_buffer[i].counter = 0;
        target_buffer[i].target_addr = 0xdeadbeef;
        target_buffer[i].branch_addr = 0xdeadbeef;
      }
      BranchPredictorBase::need_fixup = false;

      fprintf(stderr,"Branch Predictor may not work, code status unknown, you probably need to check that proper code paths exists to use this...\n");
      assert(0 && "Attempting to use deprecated branch predictor code!");

    }
    ~BranchPredictorGShare() { }

    //Get predicted next PC "for real" (update the predictor)
    uint32_t get_nPC(uint32_t currPC);

    /* Allows for a check without updating the predictor */
    uint32_t peek_nPC(uint32_t currPC);

    /* Called when prediction is made */
    inline uint32_t get_gHistory() { return (need_fixup ? fixupGH : ghistory); }

    void update(uint32_t currPC, uint32_t correctPC, bool was_bad_predict, 
                  bool taken, bool is_branch, uint32_t instr_gh);

    //Overloaded assignment operator (intelligently copies state)
    BranchPredictorGShare &operator=(BranchPredictorGShare &bpb);

  private:
    //Used to figure out which table entry to look at, given the PC and global history
    uint32_t gshare_hash(uint32_t addr, uint32_t ghist);

    //Used for fast modulus operations to make hashing faster
    static const uint32_t TABLE_MASK;
    static const uint32_t HIST_MASK;

    //Table where predictor state is stored
    GShareTableEntry target_buffer[rigel::predictor::GSHARE_SIZE];
    uint32_t next_predPC;
    uint32_t ghistory;
    uint32_t fixupPC;   // Where to start fetching from after mispredict
    uint32_t fixupGH;   // The correct global history

};

#endif //#ifndef __BPREDICT_H__
