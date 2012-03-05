#ifndef __INTERVAL_SKIP_LIST_H__
#define __INTERVAL_SKIP_LIST_H__

//#include <CGAL/Interval_skip_list.h>
#include "util/cgal_isl.h"
#include "RandomLib/Random.hpp"
//#include <CGAL/Interval_skip_list_interval.h>
#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <algorithm>
#include <ostream>

const unsigned int PERM1 = 0x1;
const unsigned int PERM0 = 0x0;
const unsigned int PERMX = 0x2;

struct ThreadSetPermissions {
  unsigned ThreadSetID : 23;
  unsigned R : 2;
  unsigned W : 2;
  unsigned X : 2;
  unsigned O : 1;
  unsigned T : 1;
  unsigned pad : 1;

  ThreadSetPermissions(unsigned int tsid, unsigned int r=PERM0, unsigned int w=PERM0, unsigned int x=PERM0, bool o=false, bool t=false) :
    ThreadSetID(tsid), R(r), W(w), X(x), O(o), T(t) { }

  bool operator==(const ThreadSetPermissions &other) const {
    return (ThreadSetID == other.ThreadSetID);
  }

  bool operator!=(const ThreadSetPermissions &other) const {
    return !(*this == other);
  }

  ThreadSetPermissions& operator=(const ThreadSetPermissions &rhs) {
    ThreadSetID = rhs.ThreadSetID;
    R = rhs.R;
    W = rhs.W;
    X = rhs.X;
    O = rhs.O;
    T = rhs.T;
    return *this;
  }

  friend std::ostream& operator<<(std::ostream &os, const ThreadSetPermissions &tsp) {
    os << "[TS" << tsp.ThreadSetID << ", ";
    os << (tsp.R == PERM1 ? "1" : (tsp.R == PERM0 ? "0" : "X"));
    os << (tsp.W == PERM1 ? "1" : (tsp.W == PERM0 ? "0" : "X"));
    os << (tsp.X == PERM1 ? "1" : (tsp.X == PERM0 ? "0" : "X"));
    os << (tsp.O ? "O" : "-");
    os << (tsp.T ? "T" : "-");
    os << "]";
    return os;
  }

};

typedef uint32_t addr_t;

class LumenInterval {
  public:
    typedef addr_t Value; //ISL uses this typedef
    typedef std::list<ThreadSetPermissions> tspset;

    LumenInterval(addr_t _minaddr=0, addr_t _maxaddr=0xFFFFFFFF) :
      minaddr(_minaddr), maxaddr(_maxaddr) { }

    const Value& inf() const {return minaddr;}
    void setinf(const Value &i) { minaddr = i; }

    const Value& sup() const {return maxaddr;}
    void setsup(const Value &s) { maxaddr = s; }

    bool inf_closed() const {return true;}

    bool sup_closed() const {return true;}

    bool contains(const Value& V) const;
    // true iff this interval is a superset of the other
    bool contains_interval(const Value& l, const Value& r) const;
    // true iff this interval is a *strict* superset of the other
    bool fully_contains_interval(const Value& l, const Value& r) const;

    bool operator==(const LumenInterval& I) const
    {
      return ((inf() == I.inf()) && (sup() == I.sup()));
    }

    bool operator!=(const LumenInterval& I) const
    {
      return !(*this == I);
    }

    //We are less than another interval if the other interval contains us (is bigger than us).
    //This operator is used to make a sorted set of LumenIntervals.
    bool operator<(const LumenInterval& I) const
    {
      return I.contains_interval(inf(), sup());
    }

    bool addTSP(const ThreadSetPermissions &_tsp) {
      if(findTSP(_tsp) != TSPEnd()) return false;
      else {
        tsp.push_back(_tsp);
        return true;
      }
    }

    tspset::iterator findTSP(const ThreadSetPermissions &_tsp) {
      return std::find(tsp.begin(), tsp.end(), _tsp);
    }

    tspset::iterator findTSP(unsigned int tsid) {
      return findTSP(ThreadSetPermissions(tsid));
    }

    tspset::const_iterator TSPCBegin() const { return tsp.begin(); }
    tspset::const_iterator TSPCEnd() const { return tsp.end(); }
    tspset::iterator TSPBegin() { return tsp.begin(); }
    tspset::iterator TSPEnd() { return tsp.end(); }


    const ThreadSetPermissions &getTSP(unsigned int tsid) {
      return *findTSP(tsid);
    }

    //FIXME Should at least have a version of this that fails if a non-owner tries to escalate privileges
    //Return true if TSP with given thread set ID existed before, else false
    bool changeTSP(const ThreadSetPermissions &_tsp) {
      tspset::iterator it = findTSP(_tsp);
      if(it == TSPEnd()) return false;
      else {
        *it = _tsp;
        return true;
      }
    }

    bool removeTSP(const ThreadSetPermissions &_tsp) {
      tspset::iterator it = findTSP(_tsp);
      if(it != TSPEnd()) {
        tsp.erase(it);
        return true;
      }
      else {
        return false;
      }
    }

    friend std::ostream& operator<<(std::ostream &os, const LumenInterval &i) {
      os << "[[INTERVAL " << (i.inf_closed()?"[":"(") << i.inf() << ", " << i.sup() << (i.sup_closed()?"]":")");
      os << " -- ";
      for(tspset::const_iterator it = i.TSPCBegin(), end = i.TSPCEnd(); it != end; ++it) {
        os << *it << " ";
      } 
      os << " ]]";
      return os;
    }
    
    addr_t minaddr, maxaddr;
    tspset tsp;
};

bool LumenInterval::contains_interval(const LumenInterval::Value& i,
              const LumenInterval::Value& s) const
  // true iff this contains (l,r)
{
  return( (inf() <= i) && (sup() >= s) );
}

bool LumenInterval::fully_contains_interval(const LumenInterval::Value& i,
              const LumenInterval::Value& s) const
{
  return( (inf() <= i) && (sup() >= s) && !((inf() == i) && (sup() == s)));
}

bool LumenInterval::contains(const LumenInterval::Value& v) const
{
  // return true if this contains V, false otherwise
  return ((v >= inf()) && (v <= sup()));
}


//=======================================================

class LumenISL : public CGAL::Interval_skip_list<LumenInterval>
{
  public:
    LumenISL() : CGAL::Interval_skip_list<LumenInterval>() {}
    template <class InputIterator>
    LumenISL(InputIterator b, InputIterator e) : 
      CGAL::Interval_skip_list<LumenInterval>(b, e) { }
    bool insert_optimized(LumenInterval& I) { //Can't be const bc we call setinf/setsup
      bool ret = false;
      addr_t minaddr = I.inf();
      addr_t maxaddr = I.sup();
      if(search(I.inf()) == NULL) {
        if(search(I.inf()+1) != NULL) {
          ret = true;
          I.setinf(I.inf()+1);
        }
        else if(search(I.inf()-1) != NULL) {
          ret = true;
          I.setinf(I.inf()-1);
        }
      }
      if(search(I.sup()) == NULL) {
        if(search(I.sup()+1) != NULL) {
          ret = true;
          I.setsup(I.sup()+1);
        }
        else if(search(I.sup()-1) != NULL) {
          ret = true;
          I.setsup(I.sup()-1);
        }
      }
      insert(I);
    }

    template <class InputIterator>
    int insert_optimized(InputIterator b, InputIterator e)
    {
      int i = 0;
      for(; b!= e; ++b){
        if(insert_optimized(*b))
          ++i;
      }
      return i;
    }

};

#if 0

int main()
{
  LumenISL isl;
  ////std::cin >> n >> d;
  size_t n = 100000;
  std::vector<LumenInterval> intervals(n);
  RandomLib::Random rng(0);
  for(size_t i = 0; i < n; i++) {
   uint32_t lowaddr = rng.IntegerC<uint32_t>(0, 1U << 25);
   uint32_t len = rng.IntegerC<uint32_t>(0, 1U << 20);
   uint32_t highaddr = ((lowaddr+len) > lowaddr) ? (lowaddr+len) : 0xFFFFFFFFU;
   intervals[i] = LumenInterval(lowaddr, highaddr);
   ThreadSetPermissions tsp(i, PERM0, PERM1, PERMX, true, false);
   intervals[i].addTSP(tsp);
   tsp.ThreadSetID++;
   tsp.X = PERM0;
   intervals[i].addTSP(tsp);
  }
  //std::random_shuffle(intervals.begin(), intervals.end());



  ///////////
  // 3 Stacked intervals (tests sorting by interval size)
  ///////////
  // size_t n = 3;
  // std::vector<LumenInterval> intervals(3);
  // intervals[0] = LumenInterval(300, 700);
  // ThreadSetPermissions tsp(20, PERMX, PERMX, PERMX, false, true);
  // intervals[0].addTSP(tsp);
  // intervals[1] = LumenInterval(0, 1000);
  // tsp.ThreadSetID = 10; tsp.R=PERM1; tsp.W=PERM1; tsp.X=PERM1; tsp.O=true; tsp.T=false;
  // intervals[1].addTSP(tsp);
  // intervals[2] = LumenInterval(400,600);
  // tsp.ThreadSetID = 30; tsp.R=PERM0; tsp.W=PERM0; tsp.X=PERM0; tsp.O=false; tsp.T=false;
  // intervals[2].addTSP(tsp);

  std::cout << isl.insert_optimized(intervals.begin(), intervals.end()) << " of " << intervals.size() << " intervals optimized\n";

  // for(size_t i = 0; i < 10*n+100; i += 10) {
  //   std::cout << i << ": ";
  //   std::vector<LumenInterval> L;
  //   isl.find_intervals(i, std::back_inserter(L));
  //   for(std::vector<LumenInterval>::iterator it = L.begin(), end = L.end(); it != end; ++it){
  //     std::cout << *it;
  //   }
  //   std::cout << std::endl;
  // }

  ////////////////////////////
  // This code does a stabbing query and sorts by interval length
  ////////////////////////////
  size_t nstabs = 1000;
  for(size_t i = 0; i < nstabs; i++) {
    std::vector<LumenInterval> L;
    uint32_t addr = rng.IntegerC<uint32_t>(0U, 0xFFFFFFFFU);
    isl.find_intervals(addr, std::back_inserter(L));
    std::cout << addr << " : ";
    for(std::vector<LumenInterval>::iterator it = L.begin(), end = L.end(); it != end; ++it){
      std::cout << *it;
    }
    std::cout << std::endl;

    std::sort(L.begin(), L.end());
    std::cout << addr << " (sorted) : ";
    for(std::vector<LumenInterval>::iterator it = L.begin(), end = L.end(); it != end; ++it){
      std::cout << *it;
    }
    std::cout << std::endl;
  }

  for(size_t i = 0; i < n; i++) {
    isl.remove(intervals[i]);
  }
  return 0;

}

#endif

#endif //#ifndef __INTERVAL_SKIP_LIST_H__
