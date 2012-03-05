
////////////////////////////////////////////////////////////////////////////////
// arbiter.h
////////////////////////////////////////////////////////////////////////////////
// Arbiter base class
// Arbiter derived classes
// Arbiter container class
//
// This is meant to provide a generic arbiter class and interface that can be
// extended to provide contention modeling for resources
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _ARBITER_H_
#define _ARBITER_H_

#include "sim.h"
#include "RandomLib/Random.hpp"

namespace rigel {
  extern RandomLib::Random RNG;
}

//#define DEBUG_ARB

////////////////////////////////////////////////////////////////////////////////
// Arbiter
////////////////////////////////////////////////////////////////////////////////
//
// This is a generic Arbiter Base Class
//
// Arbitrates for a single resource type
//
// Keeps track of a list of:
//   1) Requests to a single resource type (there may be multiple interchangeable
//   resources of the same type the allocate, such as multiple read equivalent
//   read ports)
//   2) Status of requests (which requests from the previous set of requests
//   succeeded)
//
// Each Cycle, grant access to each resource to one of the requestors
//
// Parameter: Cycles until resolution
//
////////////////////////////////////////////////////////////////////////////////
class Arbiter
{
  private:

    int res_max;  // indicate how many of each resource is available
                  // this is a template parameter "constant" or #defined
    int accessors; // how many accessors contend for this resource
    int resources; // indicates current resource availability internally
                  // array ofcounters for resources reset to res_quantities each cycle
    int requests; // how many requests outstanding (unserviced)

  public:

    int *status;    // indicates end resource availability externally
                    // one entry per accessor/input to arbitrator
    int *reqs;      // same size as status
                    // one entry per accessor/input to arbitrator
    ARB_POLICY arb_policy;
    int backpressure; // indicates how many of the resources are stalled ahead

    int next_p; // next priority

    // private helper to get next priority
    // this should be rolled into some arbitration policy object
    int get_next_priority(int p) {
      return (p+1)%accessors; // next priority for round robin
    }
    int get_random_priority() {
      return rigel::RNG.Integer(accessors); // next priority for random
    }

  // destructor
  ~Arbiter() {
    delete[] reqs;
    delete[] status;
  }

  // constructor
  Arbiter( int num_acc, int num_res, ARB_POLICY arb_p ) {
    res_max = num_res;
    accessors = num_acc;
    resources = res_max;
    requests = 0;
    reqs = new int[num_acc];
    status = new int[num_acc];
    for(int i=0;i<num_acc;i++) {
      reqs[i] = 0;
      status[i] = 0;
    }
    arb_policy = arb_p;
    next_p = 0; // next priority for round-robin
    backpressure = 0;
  }

  // set
  void SetArbPolicy( ARB_POLICY p ) {
    arb_policy = p;
  }

  // request a resource for a particular accessor
  void MakeRequest(int acc_id ) {
    reqs[acc_id]++;
    requests++; // log a new request
    #ifdef DEBUG_ARB
    DEBUG_HEADER();
    cerr << "\nRequest logged for accessor " << acc_id << " Total reqs " << requests << std::"\n";
    #endif
  }

  // stall a resource for this arbitration cycle
  void ApplyBackPressure() {
    backpressure++;
  }

  // arbitrate resources amound current requests based on set policy
  void Arbitrate() {
    //printf("%d Resources\n", resources);
    #ifdef DEBUG_ARB
    cerr << "  Arbiter::Arbitrate()" << "\n";
    #endif
    //NOTE For pseudo-MT, modifying arbitration so resources are not freed until
    //ClaimResource() is called.
    //int tempres = resources - backpressure;
    //OLD COMMENT:
    // assumes we can grant each resource each time we arbitrate
    // will need modification to support non-unity length ownership
    resources = res_max - backpressure;

    // at most, we need allocate each resource once, so iterate once per
    // available resource
    int p, grants=0;
    switch (arb_policy) {
      case ARB_ROUND_ROBIN: {
        for(int i=0;i<res_max;i++) {
          // as long as we still have resources to allocate and requests to service...
          if(resources<=0 || requests<=0){
            break; // done
          }
          else {
            // get_next_priority accessor until we find someone who wants a resource
            p = next_p;
            do{
              if(reqs[p]>0) { // if next priority accessor made a request
		#ifdef DEBUG_ARB
                cerr << "Granting to " <<  p << std::"\n";
		#endif
                reqs[p]--;    // grant request
                requests--;   // serviced a request (track total usage)
                status[p]++;  // grant resource to accessor 'p'
                resources--;   // took a resource  (track total usage)
                            // bp_applied_HACK += requests;
                assert(reqs[p]>=0 && "Error, grant made when no request!");
                grants++;
                #ifdef DEBUG_ARB
                cerr << "  : Arb to accessor " << p << "\n";
                #endif
              }
              p = get_next_priority(p);
            } while( resources>0 && requests>0 );
          }
        }
        break;
      }
      case ARB_RANDOM: {
        for (int i = 0; i < res_max; i++) {
          if(resources<=0 || requests<=0){
            break; // done
          }
          while( reqs[p = get_random_priority()] <= 0 );
          assert(reqs[p] > 0 && "Error, grant made when no request!");
          reqs[p]--;
          requests--;
          status[p]++;
          resources--;

          grants++;
        }
        break;
      }
  }


    #ifdef DEBUG_ARB
    if(grants>0)
      cerr << "  : granted " << grants << " resources" << "\n";
    #endif

    assert(resources >=0 && requests >=0 && "Error in Resource Grants!");

    // update arbitration policy
    // the policy may keep state from this cycle, or may not
    // current simple policy: clear all requests and restart next cycle
    // requests = 0; done in ClearArbRequests called by PerCycle
    //resources = tempres + backpressure;
    backpressure = 0;
    next_p = (next_p+1)%accessors; // next priority for round robin policy
    #ifdef DEBUG_ARB
    cerr << "  : next_p priority " << next_p << "\n";
    #endif

  };

  // Return Arbitration Status for this ID and resource request
  int GetArbStatus( int acc_id  ){
    return status[acc_id]; // return grant count for requested resource
  };

  // clear out last cycle's arb status
  void ClearArbStatus(){
    #ifdef DEBUG_ARB
    DEBUG_HEADER();
    cerr << "Clearing All ARB status (gnts)\n";
    #endif
    for(int i=0;i<accessors;i++){
      status[i]=0;
    }
  };

  // clear out last cycle's arb requests
  void ClearArbRequests(){
    #ifdef DEBUG_ARB
    DEBUG_HEADER();
    cerr << "Clearing All ARB reqs\n";
    #endif
    for(int i=0;i<accessors;i++){   // clear all requests
      reqs[i]=0;
    }
    requests = 0;
  };

  // Decrement status grant counter to "claim" an awarded resource and
  // prevent it from being used more than once
  void ClaimResource( int acc_id ){
    assert(status[acc_id] > 0 && "Attempting to Claim Unavailable Resource");
    //printf("%d releasing\n", acc_id);
    status[acc_id]--;
    //resources++;
  };

  void PrintStatus() {
    if(requests>0) {
			std::cerr << "Arbiter status at cycle: " << rigel::CURR_CYCLE << "\n";
			std::cerr << "Resources: " << resources << "\n";
			std::cerr << "Requests:  " << requests  << "\n";
			std::cerr << "Accessors: " << accessors << "\n";
      for(int i=0;i<accessors;i++){
				std::cerr << " acc: " << i << " reqs: " << reqs[i] << " status: " << status[i] << "\n";
      }
    }
  }

};

////////////////////////////////////////////////////////////////////////////////
// ArbContainer
////////////////////////////////////////////////////////////////////////////////
// A class for collectively managing arbiters for a resource class such as a
// set of read ports for a collection of cache banks
// Contains an array of Arbiter objects, one per distinct resource
// Ex: a 4-banked cache with 2 read ports per bank would use have 4 arbiters
// that each arbitrate for their bank's 2 read ports
//
class ArbContainer {

  public:

  Arbiter** arbs; // array of arbiters for objects
  int num_arbs;  // keep count of the number of arbiters we contain

  // constructor
  // specify the number of arbiters required (number of distinct resources)
  // the number of accessors per resource
  // the number of each resource
  // the arbitration policy
  ArbContainer( int n_arbs, int n_acc, int n_res, ARB_POLICY policy ) {
    num_arbs = n_arbs;
    arbs = new Arbiter*[n_arbs];
    for(int i=0; i< n_arbs; i++){
      arbs[i] = new Arbiter(n_acc, n_res, policy);
    }
    #ifdef DEBUG_ARB
    cerr << "ArbContainer Constructor num_arbs=" << num_arbs << " num_resource=" << n_res << std::"\n";
    #endif
  };

  // extended constructor
  // optionally provide an array that specifies the number of each resource per
  // arbiter (allows for nonuniform resources)
  ArbContainer( int n_arbs, int n_acc, int *n_res, ARB_POLICY policy );

  // destructor
  ~ArbContainer( ){
    for(int i=0; i< num_arbs; i++){
      delete arbs[i];
    }
    delete[] arbs;
  }

  // arbitrate for each resource once per cycle
  void PerCycle() {
    #ifdef DEBUG_ARB
    cerr << "ArbContainer::PerCycle()" << "\n";
    #endif
    for(int i=0; i<num_arbs; i++) {
      #ifdef DEBUG_ARB
      arbs[i]->PrintStatus();
      #endif
      arbs[i]->ClearArbStatus();// clear out last cycles arb results
      arbs[i]->Arbitrate();
      arbs[i]->ClearArbRequests(); // clear out requests after arb
    }
  }

  // set arbitration policy for each unit in ArbContainer
  // sets them all the same
  void SetArbPolicy( ARB_POLICY p ) {
    for(int i=0; i<num_arbs; i++) {
      arbs[i]->SetArbPolicy(p);
    }
  }

  // request a particular resource for a particular
  // resource id and accessor id (type is used in derived classes
  void MakeRequest(int res_id, int acc_id, RES_TYPE type = NONE ) {
    #ifdef DEBUG_ARB
    cerr << "ArbContainer::MakeRequest() resource: " << res_id
      << " accessor: " << acc_id << " cycle: " << rigel::CURR_CYCLE << "\n";
    #endif
    arbs[res_id]->MakeRequest(acc_id);
  }

  //Mapping for outputs ports  based on input paramater
  // Index into Resource array
  // make this virtual (or not if we actually use ArbContainers instead of only
  // as base class?)
  int GetResourceID(uint32_t val, RES_TYPE type = NONE ) const {
    #ifdef DEBUG_ARB
    cerr << "ArbContainer GetResourceID addr=0x" << std::hex << val << std::dec << std::"\n";
    #endif
    return (val % num_arbs); // assumes simple resource assignment
  };

  // Return Arbitration Status for this accessor ID and resource id
  int GetArbStatus( int res_id, int acc_id, RES_TYPE type = NONE  ){
    #ifdef DEBUG_ARB
    cerr << "ArbContainer::GetArbStatus() status: "
      << arbs[res_id]->GetArbStatus(acc_id)
      << " resource: " << res_id << " accessor: " << acc_id << " cycle: " << rigel::CURR_CYCLE << "\n";
    #endif
    return arbs[res_id]->GetArbStatus(acc_id); // return grant count for requested resource
  };

  // Claim a resource for this accessor id and resource id
  void ClaimResource( int res_id, int acc_id, RES_TYPE type = NONE ) {
    #ifdef DEBUG_ARB
    cerr << "ArbContainer::ClaimResource() "
      << " resource: " << res_id << " accessor: " << acc_id << " cycle: " << rigel::CURR_CYCLE << "\n";
    #endif
    arbs[res_id]->ClaimResource(acc_id);
  }

  // Back Pressure
  // the arbiter needs to know the state of each resource (availability) to
  // determine if it is "allowed" to grant it this cycle
  // This models backpressure
  // apply backpressure to stall a resource for this arbitration cycle
  void ApplyBackPressure( int res_id, RES_TYPE type = NONE ) {
    arbs[res_id]->ApplyBackPressure();
  }

};

////////////////////////////////////////////////////////////////////////////////
// BusArbiter
////////////////////////////////////////////////////////////////////////////////
// A wrapper class for the "shared bus"-based cluster-level interconnect
// Contains three ArbContainers, one for read, read return,  and one for write
// Wraps most of ArbContainer's functions to pass reads and writes through
class BusArbiter {

  public:
    // constructor calls parent class constructor
    // parameters: number of read banks, number of read ports per bank
    //             number of write banks, number of write ports per bank
    //             number of accessors to arbitrate among
    BusArbiter( int n_arbs_read, int n_res_read, int n_arbs_write, int n_res_write,
                int n_acc, ARB_POLICY policy ) :
      ArbReadCmd(n_arbs_read, n_acc, n_res_read, policy ) , // Read Port Arbiter
      ArbReadReturn(n_arbs_read, n_acc, n_res_read, policy ) , // Read Port Arbiter
      ArbWrite(n_arbs_write, n_acc, n_res_write, policy ) // Write Port Arbiter
    {
      num_write_banks = n_arbs_write;
      #ifdef DEBUG_ARB
      cerr << "BusArbiter Constructor accessors=" << n_acc << std::"\n";
      #endif
    };

    // GetResourceID
    // ResourceID is based on the address to simulate L2 banks
    int GetResourceID(uint32_t val, RES_TYPE type) const { 
      #ifdef DEBUG_ARB
			std::cerr << "BusArbiter GetResourceID Addr 0x" << std::hex << val << " type " << std::dec << type << "\n";
      #endif
      return 0; 
    };

    // PerCycle Wrapper
    void PerCycle() {
      ArbReadCmd.PerCycle();
      ArbReadReturn.PerCycle();
      ArbWrite.PerCycle();
    }

    // set arbitration policy for each unit in ArbContainer
    // sets them all the same
    void SetArbPolicy( ARB_POLICY p ) {
      ArbReadCmd.SetArbPolicy(p);
      ArbReadReturn.SetArbPolicy(p);
      ArbWrite.SetArbPolicy(p);
    }

    // MakeRequest Wrapper
    void MakeRequest(int res_id, int acc_id, RES_TYPE type ) {
      //type = L2_READ; // hack for 1 RW port
      switch (type)
      {
        case L2_READ_CMD:
          ArbReadCmd.MakeRequest(res_id, acc_id);
          break;
        case L2_READ_RETURN:
          ArbReadReturn.MakeRequest(res_id, acc_id);
          break;
        case L2_WRITE:
          ArbWrite.MakeRequest(res_id, acc_id);
          break;
        default:
          assert(false && "Unknown RES_TYPE");
      }
    };

    // GetArbStatus Wrapper
    int GetArbStatus( int res_id, int acc_id, RES_TYPE type  ){
     // type = L2_READ; // hack for 1 RW port
      switch (type)
      {
        case L2_READ_CMD: return ArbReadCmd.GetArbStatus(res_id, acc_id);
        case L2_READ_RETURN: return ArbReadReturn.GetArbStatus(res_id, acc_id);
        case L2_WRITE: return ArbWrite.GetArbStatus(res_id, acc_id);
        default:
          assert(false && "Unknown RES_TYPE");
      }
      return 0;
    };

    // ClaimResource Wrapper
    void ClaimResource( int res_id, int acc_id, RES_TYPE type ) {
      //type = L2_READ; // hack for 1 RW port
      switch (type)
      {
        case L2_READ_CMD:
          ArbReadCmd.ClaimResource(res_id, acc_id);
          break;
        case L2_READ_RETURN:
          ArbReadReturn.ClaimResource(res_id, acc_id);
          break;
        case L2_WRITE:
          ArbWrite.ClaimResource(res_id, acc_id);
          break;
        default:
          assert(false && "Unknown RES_TYPE");
      }
    }

  private:
    // Command bus for reads.  Addr-sized.
    ArbContainer ArbReadCmd; 
    // For reads on the shared bus, cacheline-sized
    ArbContainer ArbReadReturn;
    // For writes on (seperate) shared bus
    ArbContainer ArbWrite; 
    int num_write_banks;

};

////////////////////////////////////////////////////////////////////////////////
// XBarArbiter
////////////////////////////////////////////////////////////////////////////////
// Arbitration wrapper for crossbar-based cluster interconnect
// Assumes combined read/write ports
class XBarArbiter : public ArbContainer {

  public:
  // constructor calls parent class constructor
  // parameters: number of read banks, number of read ports per bank
  //             number of write banks, number of write ports per bank
  //             number of accessors to arbitrate among
  XBarArbiter( int n_arbs_read, int n_res_read, int n_arbs_write, int n_res_write,
               int n_acc, ARB_POLICY policy ) :
    ArbContainer( n_arbs_read, n_acc, n_res_read, policy )
  {
    if(rigel::cache::L2D_SHARED_RW_PORTS){
      assert( n_arbs_read==n_arbs_write && "Unmatched Bank Counts for Shared RW Ports!" );
      assert( n_res_read==n_res_write && "Unmatched Port-per-bank Counts for Shared RW Ports!" );
    }else{
      assert(false && "Using Non-shared L2 ports on crossbar cluster interconnect unsupported currently");
    }
    num_write_banks = n_arbs_write;
  };

  // Get ResourceID
  // ResourceID is based on the address to simulate L2 banks
  int GetResourceID(uint32_t val, RES_TYPE type) const {
    int id = val % num_write_banks ; // select bank based on LSBs of address
    return id;
  };

  private:
    int num_write_banks;
};

#endif

