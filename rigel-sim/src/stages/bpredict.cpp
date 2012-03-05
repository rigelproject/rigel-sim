////////////////////////////////////////////////////////////////////////////////
// bpredict.cpp (Branch Predictors)
////////////////////////////////////////////////////////////////////////////////
//
// Implementation for branch predictors
//
////////////////////////////////////////////////////////////////////////////////

#include <assert.h>                     // for assert
#include <stdint.h>                     // for uint32_t
#include "core/bpredict.h"       // for BranchPredictorGShare, etc
#include "sim.h"            // for GSHARE_HIST_LENGTH, etc

//Bitmasks to make GShare table lookup faster
const uint32_t
BranchPredictorGShare::HIST_MASK = ~(0xFFFFFFFF << rigel::predictor::GSHARE_HIST_LENGTH);
const uint32_t
BranchPredictorGShare::TABLE_MASK= ~(0xFFFFFFFF << rigel::predictor::GSHARE_ORDER);

////////////////////////////////////////////////////////////////////////////////
// BranchPredictorGShare::gshare_hash()
////////////////////////////////////////////////////////////////////////////////
// Return the hash value for the PC+history pair.
// Looks at the two most recent branches
uint32_t 
BranchPredictorGShare::gshare_hash(uint32_t addr, uint32_t ghist) {
  return ((addr >> 1) ^ (0x03 & ghist)) & TABLE_MASK;
}

////////////////////////////////////////////////////////////////////////////////
// BranchPredictorGShare::update()
////////////////////////////////////////////////////////////////////////////////
// After each branch completes, update the state of the predictor.
void 
BranchPredictorGShare::update(uint32_t currPC, uint32_t correctPC, 
  bool was_bad_predict, bool taken, bool is_branch,  uint32_t instr_gh)
{
  /* Update the table index is the history used to predict this branch */
  uint32_t index = gshare_hash(currPC, instr_gh);

  if (was_bad_predict) { 
    // is_branch should be true since all non-branches on the correct path fetch
    // PC+4 and cannot have a mismatch at execute
    assert(true == is_branch);

    /* Next prediction will put us on the correct path */
    need_fixup = true;
    fixupPC = correctPC;
    fixupGH = instr_gh;

    // Set LSBit of global history as taken or not taken, shift left
    if (target_buffer[index].valid) {
      // Was *mis*predicted
      instr_gh = (instr_gh << 1);
      if (taken) { instr_gh |= 0x1; }
      fixupGH = instr_gh;
    } else {
      // Was *not* predicted, need to fix
      fixupGH = fixupGH << 1;
      if (taken) {  fixupGH |= 0x01; }
    }

    // Index used for prediction (is different than initial instr_gh)
    index = gshare_hash(currPC, instr_gh);

    // Counter update
    if (target_buffer[index].counter > 0) {
      target_buffer[index].counter--;
    }

    /* Counter update */
    if (target_buffer[index].counter < 2 || target_buffer[index].valid == false) {
      target_buffer[index].valid = true; // Replace entry
      target_buffer[index].counter = 2;
      target_buffer[index].branch_addr = currPC;
      target_buffer[index].target_addr = correctPC;
    }
  }
  else { //Was not a mispredict
    if (is_branch) {
      /* Update Smith counter */
      if (target_buffer[index].valid == false) {
        target_buffer[index].counter = 0x02;
        target_buffer[index].valid = true;
        target_buffer[index].branch_addr = currPC;
        target_buffer[index].target_addr = correctPC;
        // FIXME Could try and correct the history, but it is a PITA
      } else {
        target_buffer[index].counter++;
        target_buffer[index].counter &= 0x03;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// BranchPredictorGShare::peek_nPC()
////////////////////////////////////////////////////////////////////////////////
// Look at what the next precticted PC would be.
uint32_t 
BranchPredictorGShare::peek_nPC(uint32_t currPC) {
  uint32_t ret;
  uint32_t index = gshare_hash(currPC, ghistory);

  //If there is a valid entry that points to this PC, return its target, else PC+4
  if (target_buffer[index].valid && (target_buffer[index].branch_addr == currPC)) {
    ret = target_buffer[index].target_addr;
  } else {
    ret = currPC + 4;
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
// BranchPredictorGShare::operator=
////////////////////////////////////////////////////////////////////////////////
// Overloaded assignment operator
// Copy over state, including the table
BranchPredictorGShare &
BranchPredictorGShare::operator=(BranchPredictorGShare &bpb) 
{
  BranchPredictorGShare *bp = (BranchPredictorGShare *)&bpb;
  /* GShare specifics */
  this->next_predPC = bp->next_predPC;
  this->ghistory = bp->ghistory;
  this->need_fixup = bp->need_fixup;
  this->fixupPC = bp->fixupPC;
  this->fixupGH = bp->fixupGH;
  for (uint32_t i = 0; i < rigel::predictor::GSHARE_SIZE; i++) {
    this->target_buffer[i] = bp->target_buffer[i];
  }
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
// BranchPredictorGShare::get_nPC()
////////////////////////////////////////////////////////////////////////////////
// Predict the next PC.  This should only be called once per prediction.
// Side effects: *Copies over fixup PC and history if needed
//               *Updates global history if we have a valid table entry for this PC
uint32_t 
BranchPredictorGShare::get_nPC(uint32_t currPC) { 
  uint32_t index = gshare_hash(currPC, ghistory);

  if(need_fixup) {
    /* ghistory was fixed at update */
    need_fixup = false;
    next_predPC = fixupPC;
    ghistory = fixupGH;
    return fixupPC;
  }

  //If we have a valid entry for this PC:
  if (target_buffer[index].valid && currPC == target_buffer[index].branch_addr) 
  {
    next_predPC = target_buffer[index].target_addr;
    ghistory = ghistory << 1;
    if (next_predPC == currPC + 4) { //Predicted not taken
      ghistory |= 0x0;
    }
    else { //Predicted taken
      ghistory |= 0x1; // Taken
    }
  }
  else { //Not a branch
    next_predPC = currPC + 4;
  }
  return next_predPC;
}
