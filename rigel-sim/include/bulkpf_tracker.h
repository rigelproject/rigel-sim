#ifndef __BULKPF_TRACKER_H__
#define __BULKPF_TRACKER_H__

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <set>
#include "sim.h"
#include "memory/address_mapping.h"

class BulkPFTracker
{
  public:
    BulkPFTracker() { }
    void addLine(uint32_t addr) {
      /*
      if(lines.empty())
      {
        controller = AddressMapping::GetController(addr);
        rank = AddressMapping::GetRank(addr);
        bank = AddressMapping::GetBank(addr);
        row = AddressMapping::GetRow(addr);
      }
      else
      {
        assert(controller == AddressMapping::GetController(addr)
               && "Trying to add address to BulkPF which spans controllers");
        assert(rank == AddressMapping::GetRank(addr)
               && "Trying to add address to BulkPF which spans ranks");
        assert(bank == AddressMapping::GetBank(addr)
               && "Trying to add address to BulkPF which spans banks");
        assert(row == AddressMapping::GetRow(addr)
               && "Trying to add address to BulkPF which spans rows");
      }
      */
      lines.insert(addr & rigel::cache::LINE_MASK);
    }
    //Mark address as handled, returns true if set is now empty.
    bool removeLine(uint32_t addr) {
      assert(lines.erase(addr & rigel::cache::LINE_MASK) == 1
             && "Address was not found in BulkPF line address set");
      return lines.empty();
    }
    //Start address, end address (inclusive)
    void addLineRange(uint32_t start, uint32_t end)
    {
      for(uint32_t i = (start & rigel::cache::LINE_MASK);
                   i <= (end & rigel::cache::LINE_MASK);
                   i += rigel::cache::LINESIZE)
        addLine(i);
    }
    void addLineRangeTruncate(uint32_t start, uint32_t end)
    {
      unsigned int c, ra, b, ro;
      c = AddressMapping::GetController(start);
      ra = AddressMapping::GetRank(start);
      b = AddressMapping::GetBank(start);
      ro = AddressMapping::GetRow(start);
      for(uint32_t i = (start & rigel::cache::LINE_MASK);
              i <= (end & rigel::cache::LINE_MASK) &&
              c == AddressMapping::GetController(i) &&
              ra == AddressMapping::GetRank(i) &&
              b == AddressMapping::GetBank(i) &&
              ro == AddressMapping::GetRow(i);
              i += rigel::cache::LINESIZE)
        addLine(i);
    }
    void addLineRangeStartLength(uint32_t start, uint32_t length)
    {
      addLineRange(start, start+length);
    }
    void addLineRangeStartLengthTruncate(uint32_t start, uint32_t length)
    {
      addLineRangeTruncate(start, start+length);
    }
    //Returns whether or not the address given is within a line currently tracked
    bool find(uint32_t addr) const
    {
      return lines.find(addr & rigel::cache::LINE_MASK) != lines.end();
    }
    void reset()
    {
      lines.clear();
    }
    void copyTo(BulkPFTracker &other) const
    {
      for(set<uint32_t>::iterator it = lines.begin(); it != lines.end(); ++it)
        other.addLine(*it);
    }
    void print() const
    {
      for(set<uint32_t>::iterator it = lines.begin(); it != lines.end(); ++it)
        fprintf(stderr, "%8X ", (*it));
      //fprintf(stderr, "\n");
    }
    set<uint32_t>::iterator begin() const
    {
      return lines.begin();
    }
    set<uint32_t>::iterator end() const
    {
      return lines.end();
    }
    set<uint32_t>::const_iterator begin() const
    {
      return lines.begin();
    }
    set<uint32_t>::const_iterator end() const
    {
      return lines.end();
    }
    bool empty() const
    {
      return lines.empty();
    }
    //Set of cache line addresses involved
    set<uint32_t> lines;
  private:
    unsigned int controller;
    unsigned int rank;
    unsigned int bank;
    unsigned int row;
};
#endif
