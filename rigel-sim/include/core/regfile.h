#ifndef __REGISTER_FILE_H__
#define __REGISTER_FILE_H__

#include <cassert>
#include <google/protobuf/repeated_field.h>
#include "typedefs.h"
#include "define.h"
#include "util/dynamic_bitset.h"

typedef ::google::protobuf::RepeatedField< RIGEL_PROTOBUF_WORD_T > RepeatedWord;

#define DB_RF 1

class regval32_t {

  public:

    /// default constructor, marks invalid 
    regval32_t() :
      valid_(false),
      isFloat_(false)
    { 
      data.u32 = 0x00000000;
    }

    /// init i32 constructor, marks valid with contents 
    regval32_t(uint32_t val) : 
      valid_(true),
      isFloat_(false)
    { 
      data.u32 = val;
    }

    // init i32 (int) constructor, marks valid with contents 
    regval32_t(int val) : 
      valid_(true),
      isFloat_(false)
    { 
      data.i32 = int32_t(val);
    }

    /// init f32 constructor, marks valid 
    regval32_t(float val, bool ro = false) :
      valid_(true),
      isFloat_(true)
    { 
      data.f32 = val;
    }

    /// assign an int
    regval32_t& operator=(const int &i) {
      data.i32 = (uint32_t)i;
      isFloat_ = false;
      valid_   = true;
      return *this;
    }

    /// assign a uint32_t
    regval32_t& operator=(const uint32_t &u) {
      data.u32 = u;
      isFloat_ = false;
      valid_   = true;
      return *this;
    }

    /// assign a float
    regval32_t& operator=(const float &f) {
      data.f32 = f;
      isFloat_ = true;
      valid_   = true;
      return *this;
    }

    /// assign a rword32_t
    regval32_t& operator=(const rword32_t &r) {
      data = r;
      // who cares about the Float status?
      valid_   = true;
      return *this;
    }

    /// allow converstion to a rword32_t union
    operator rword32_t() {
      return data;
    }

    void setFloat()   { isFloat_ = true;  }
    void setInt()     { isFloat_ = false; }
    void invalidate() { valid_   = false; }

    bool isFloat() const { return isFloat_; }
    bool isInt()   const { return !isFloat_; }
    bool valid()   const { return valid_; }

    int32_t  i32() const { return data.i32; }
		uint32_t u32() const { return data.u32; }
    float    f32() const { return data.f32; }

  //public data members
  private:
    rword32_t data; /// Register data union
    bool valid_;
    bool isFloat_;  /// track if this value is a float

};

// this could be templatized for future support of arbitrary register files 
class RegisterFile {

  public:

    /// constructor
    RegisterFile(unsigned int numregs);

    /// destructor
    ~RegisterFile(); 

    /// accessor
    /// return a constant regval32_t (to disallow assignment via [])
    const regval32_t operator[](uint32_t i) const;

    /// write a register
    void write(unsigned int i, regval32_t rv, uint32_t pc);

    /// Dump, print RF contents
    void Dump(unsigned int cols = 8);

    /// set trace file
    void setTraceFile( const char* filename );

    virtual void save_to_protobuf(RepeatedWord *ru) const {
      for(unsigned int i = 0; i < numregs; i++)
        ru->Add(rf[i].u32());
    }
    //FIXME I don't think this will work to restore regfiles with read-only registers.
    //We may need to add a flag or private member function to allow mutating these (like set()
    //in RegisterFileLegacy).
    virtual void restore_from_protobuf(RepeatedWord *ru) {
      assert(ru->size() == (int)numregs && "Protobuf has different # of regs than RF object");
      for(unsigned int i = 0; i < numregs; i++)
        rf[i] = ru->Get(i);
    }

  protected:

    unsigned int numregs;
    regval32_t* rf; /// the register file 

    FILE* tracefile;

  private:
   
    // copies are bad with dynamic memory... 
    RegisterFile(const RegisterFile& r);
};

/// special purpose register file
/// currently, includes/rigellib.h defines the SPRF contents
//     SPRF_CORE_NUM = 0,            // Unique core number for the chip [0..N-1]
//     SPRF_CORES_TOTAL = 1,         // Total number of cores on the chip (N).
//     SPRF_CORES_PER_CLUSTER = 2,   // Number of cores in a cluster, nominally 8.  TODO: This may become threads, but we may also want to not overload it and leave it as cores/cluster.
//     SPRF_THREADS_PER_CLUSTER = 3, // Total number of threads per cluster
//     SPRF_CLUSTER_ID = 4,          // Unique cluster number for the chip [0..N/(cores per cluster)-1].
//     SPRF_NUM_CLUSTERS = 5,        // Total number of clusters.
//     // Not used yet.
//     SPRF_THREAD_NUM = 6,          // thread id (global?)
//     SPRF_THREADS_TOTAL = 7,       // total thread count (global?)
//     SPRF_THREADS_PER_CORE = 8,    // threads on each core
//     SPRF_SLEEP = 9,               // sleepy cores
//     SPRF_CURR_CYCLE_LOW = 10,     // cycle count 32LSBs
//     SPRF_CURR_CYCLE_HIGH = 11,    // cycle count 32MSBs
//     SPRF_ENABLE_INCOHERENT_MALLOC = 12, 
//     SPRF_HYBRIDCC_BASE_ADDR = 13,  // Startup code allocates a region of the address space to use for the hybrid coherence fine-grain region table.
//     SPRF_NONBLOCKING_ATOMICS = 14, // Enable/disable non-blocking atomics for the thread.  If enabled, only the CCache must take the request before proceeding, no need to wait for a
//     // response.
//     SPRF_STACK_BASE_ADDR = 15,    // Set the minimum stack address.  Only the minimum value is kept so it can be called multiple times.  It is called from crt0.S.
//     SPRF_IS_SIM = 16,             // true (1) in the simulator
//     SPRF_NUM_SREGS                // Total number of SPRF's.
//
class SPRegisterFile : public RegisterFile {
    
  public:

    /// constructor
    SPRegisterFile(unsigned int numregs)
      : RegisterFile(numregs),
        writable(numregs)
    {
      // initialize via assign() at sim startup, or within target code with mtsr
      rf[SPRF_CORE_NUM]                 = 0; 
      rf[SPRF_CORES_TOTAL]              = 0;
      rf[SPRF_CORES_PER_CLUSTER]        = 0;
      rf[SPRF_THREADS_PER_CLUSTER]      = 0;
      rf[SPRF_CLUSTER_ID]               = 0;
      rf[SPRF_NUM_CLUSTERS]             = 0;
      rf[SPRF_THREAD_NUM]               = 0; 
      rf[SPRF_THREADS_TOTAL]            = 0;
      rf[SPRF_THREADS_PER_CORE]         = 0;
      rf[SPRF_SLEEP]                    = 1;
        writable.set(SPRF_SLEEP);
      rf[SPRF_CURR_CYCLE_LOW]           = 0;
      rf[SPRF_CURR_CYCLE_HIGH]          = 0;
      rf[SPRF_ENABLE_INCOHERENT_MALLOC] = 0;
      rf[SPRF_HYBRIDCC_BASE_ADDR]       = 0;
        writable.set(SPRF_HYBRIDCC_BASE_ADDR);
      rf[SPRF_NONBLOCKING_ATOMICS]      = 0;
        writable.set(SPRF_NONBLOCKING_ATOMICS);
      rf[SPRF_STACK_BASE_ADDR]          = 0;
        writable.set(SPRF_STACK_BASE_ADDR);
      rf[SPRF_IS_SIM]                   = 1;
      // ARGC for commandline arguments to main()
      rf[SPRF_ARGC]                     = (uint32_t) rigel::TARGET_ARGS.size();
      // ARGV for commandline arguments to main()
      // FIXME/todo: remove hardcoded constant
      rf[SPRF_ARGV]                     = (uint32_t) (0x400000-(4*rigel::TARGET_ARGS.size()));
    }

    /// READ-ONLY accessor
    const regval32_t operator[](unsigned int i) const {

      // handle special case SPRF register reads
      switch (i) {
        // 64-bit cycle count readout
        case SPRF_CURR_CYCLE_LOW:
			    return regval32_t((uint32_t)(rigel::CURR_CYCLE & 0xFFFFFFFFU));
          break;
        case SPRF_CURR_CYCLE_HIGH:
          return regval32_t((uint32_t)((rigel::CURR_CYCLE >> 32) & 0xFFFFFFFFU));
          break;
        // read the sprf register if it exists
        default:
          if (i >= rigel::NUM_SREGS) {
            printf("invalid SPRF id %d\n", i);
            assert(0 && "invalid SPRF identifier");
          } else {
            return rf[i];  
          }
          break;
      }
      assert(0 && "unreachable!");
      return 0;
    }

    /// assign: for assigning at init time and internal use by the simulator ONLY
    /// NOTE: allows arbitrary clobbering of SPRF values
    /// NOTE: NOT for use during runtime simulation. use write()
    void assign(sprf_names_t i, uint32_t val);

    // for writing the SPRF during simulation
    void write(unsigned int i, regval32_t rv, uint32_t pc);

    private:

      DynamicBitset writable; /// keep track of which registers may be written

};

#endif
