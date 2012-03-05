#ifndef __LOCALITY_TRACKER_H__
#define __LOCALITY_TRACKER_H__

#include "memory/address_mapping.h"
#include <string>
#include <iostream>
#include <list>
#include <cstdio>
#include <cstring>
#include <climits>

typedef enum {
  LT_POLICY_MAP,
  LT_POLICY_FIFO,
  LT_POLICY_MCF,
  LT_POLICY_RR
} lt_policy_t;

typedef struct _lt_entry
{
  _lt_entry() : row(0),mapCounter(0),accessCounter(0) {}
  _lt_entry(unsigned int _row, unsigned int _accessCounter) : row(_row),
    mapCounter(0), accessCounter(_accessCounter) {}
  unsigned int row;
  unsigned int mapCounter;
  unsigned int accessCounter;
} lt_entry;

class LocalityTracker
{
  public:
    LocalityTracker(std::string _name, unsigned int _windowSize, lt_policy_t _policy,
                    unsigned int _channels, unsigned int _ranks,
                    unsigned int _banks, unsigned int _rows) : name(_name),
                    windowSize(_windowSize), policy(_policy),
                    numAccesses(0UL), numConflicts(0UL),
                    channels(_channels), ranks(_ranks), banks(_banks), rows(_rows),
                    roundRobinIndex(0), accessCounter(0), lastController(UINT_MAX),
                    lastRank(UINT_MAX), lastBank(UINT_MAX), lastRow(UINT_MAX),
                    consecutiveRowHits(0UL)
    {
      if(policy == LT_POLICY_MCF || policy == LT_POLICY_MAP)
      {
        histogramScratch = (unsigned int *)calloc(_rows, sizeof(*histogramScratch));
        if(histogramScratch == NULL)
        {
          printf("Error: Could not calloc() histogramScratch (Out Of Memory?)\n");
          exit(1);
        }
      }
      openRow = new unsigned int**[channels];
      window = new std::list<lt_entry> **[channels];
      for(unsigned int i = 0; i < channels; i++)
      {
        openRow[i] = new unsigned int *[ranks];
        window[i] = new std::list<lt_entry> *[ranks];
        for(unsigned int j = 0; j < ranks; j++)
        {
          openRow[i][j] = new unsigned int[banks];
          window[i][j] = new std::list<lt_entry> [banks];
          //Initialize all entries to "rows" so that the first access of each bank will be a conflict
          //(row indices are in [0,N-1] )
          for(unsigned int k = 0; k < banks; k++)
            openRow[i][j][k] = rows;
        }
      }
    }

    ~LocalityTracker()
    {
      if(policy == LT_POLICY_MCF || policy == LT_POLICY_MAP)
        free(histogramScratch);
      for(unsigned int i = 0; i < channels; i++)
      {
        for(unsigned int j = 0; j < ranks; j++)
        {
          delete[] openRow[i][j];
          delete[] window[i][j];
        }
        delete[] openRow[i];
        delete[] window[i];
      }
      delete[] openRow;
      delete[] window;
    }

    void addAccess(uint32_t addr)
    {
      numAccesses++;
      unsigned int controller = AddressMapping::GetController(addr);
      unsigned int rank = AddressMapping::GetRank(addr);
      unsigned int bank = AddressMapping::GetBank(addr);
      unsigned int row = AddressMapping::GetRow(addr);

      if(window[controller][rank][bank].size() >= windowSize) //Full
      {
        handleFull(window[controller][rank][bank], openRow[controller][rank][bank]);
      }
      lt_entry newEntry(row, accessCounter++);
      //Fill in MAP counter if necessary
      if(policy == LT_POLICY_MAP)
      {
        bool found = false;
        for(std::list<lt_entry>::iterator it = window[controller][rank][bank].begin();
                                     it != window[controller][rank][bank].end();
                                     ++it)
        {
          if((*it).row == row)
          {
            found = true;
            newEntry.mapCounter = (*it).mapCounter;
            break;
          }
        }
        if(!found)
          newEntry.mapCounter = 0;
      }
      window[controller][rank][bank].push_back(newEntry);
      if(lastController == controller && lastRank == rank && lastBank == bank && lastRow == row)
      {
        //printf("%s CRH!\n", name.c_str());
        consecutiveRowHits++;
      }
      else
      {
        //printf("%s CRM, %f%%\n", name.c_str(), getConsecutiveHitRate());
        lastController = controller;
        lastRank = rank;
        lastBank = bank;
        lastRow = row;
      }
    }

    void handleFull(std::list<lt_entry> &thisWindow, unsigned int &thisOpenRow)
    {
      //Drain hits
      std::list<lt_entry>::iterator it = thisWindow.begin();
      unsigned int numHits = 0;
      while(true)
      {
        if(it == thisWindow.end()) break;
        if((*it).row == thisOpenRow) //Hit, service it
        {
          it = thisWindow.erase(it);
          numHits++;
        }
        else //Miss, keep looking
          ++it;
      }
      if(numHits != 0) //If we got some hits, we have free window entries to fill.
        return;
      //If we got no hits, we need to open a new row to drain some requests.
      numConflicts++;
      thisOpenRow = findNewRow(thisWindow);
      numHits = 0;
      it = thisWindow.begin();
      while(true)
      {
        if(it == thisWindow.end()) break;
        if((*it).row == thisOpenRow) //Hit, service it
        {
          it = thisWindow.erase(it);
          numHits++;
        }
        else //Miss, keep looking
          ++it;
      }
      assert(numHits != 0 && "no row hits on secondary drain");
    }

    void reset(bool closeAllRows)
    {
      if(closeAllRows)
      {
        for(unsigned int i = 0; i < channels; i++)
          for(unsigned int j = 0; j < ranks; j++)
            for(unsigned int k = 0; k < banks; k++)
              openRow[i][j][k] = rows;
      }
      numAccesses = 0UL;
      numConflicts = 0UL;
    }

    inline void dump()
    {
      drain();
      printf("%s %"PRIu64" Accesses, %"PRIu64" Hits, %"PRIu64" Conflicts, %f%% hit rate, %f%% consecutive hit rate\n",
              name.c_str(), getNumAccesses(), getNumHits(), getNumConflicts(), getHitRate(), getConsecutiveHitRate());
    }

    inline void drain()
    {
      for(unsigned int i = 0; i < channels; i++)
        for(unsigned int j = 0; j < ranks; j++)
          for(unsigned int k = 0; k < banks; k++)
            while(!window[i][j][k].empty())
              handleFull(window[i][j][k], openRow[i][j][k]);
    }

    inline uint64_t getNumAccesses() const { return numAccesses; }
    inline uint64_t getNumConflicts() const { return numConflicts; }
    inline uint64_t getNumHits() const { return numAccesses - numConflicts; }
    inline float getHitRate() const { return (float)((double)(numAccesses - numConflicts)/(numAccesses))*100.0f; }
    inline float getConsecutiveHitRate() const { return (float)((double)(consecutiveRowHits)/(numAccesses))*100.0f; }

    unsigned int findNewRow(std::list<lt_entry> &thisWindow )
    {
      switch(policy)
      {
        case LT_POLICY_FIFO:
        {
          std::list<lt_entry>::iterator it = thisWindow.begin();
          return ((*it).row);
        }
        case LT_POLICY_RR:
        {
          //Usually thisWindow.size() == windowSize, but not when we are draining at the end of a run.
          //This should make sure that we don't try to refer to a request that doesn't exist at the time.
          //This is also why we % by thisWindow.size() instead of windowSize at the end.
          if(roundRobinIndex >= thisWindow.size())
          {
            std::list<lt_entry>::iterator it = thisWindow.end();
            --it;
            return (*it).row;
          }
          std::list<lt_entry>::iterator it = thisWindow.begin();
          for(unsigned int i = 0; i < roundRobinIndex; i++)
            ++it;
          roundRobinIndex = (roundRobinIndex + 1) % thisWindow.size();
          return (*it).row;
        }
        case LT_POLICY_MCF:
        {
          //Clear histogram scratch area
          memset(histogramScratch, 0, rows*sizeof(*histogramScratch));
          //Build histogram of row frequencies in buffer, select most common
          for(std::list<lt_entry>::iterator it = thisWindow.begin();
                                       it != thisWindow.end();
                                       ++it)
          {
            histogramScratch[(*it).row]++;
          }
          unsigned int max = 0, maxrow = thisWindow.begin()->row;
          for(unsigned int i = 0; i < rows; i++)
          {
            if(histogramScratch[i] > max)
            {
              max = histogramScratch[i];
              maxrow = i;
            }
          }
          return maxrow;
        }
        case LT_POLICY_MAP:
        {
          //Clear histogram scratch area
          memset(histogramScratch, 0, rows*sizeof(*histogramScratch));
          //Choose row w/ highest penalty counter
          unsigned int max = 0, maxrow = thisWindow.begin()->row;
          for(std::list<lt_entry>::iterator it = thisWindow.begin();
                              it != thisWindow.end();
                              ++it)
          {
            if((*it).mapCounter > max)
            {
              max = (*it).mapCounter;
              maxrow = (*it).row;
            }
          }
          //Build histogram of row frequencies, add to running penalty counters
          for(std::list<lt_entry>::iterator it = thisWindow.begin();
                                       it != thisWindow.end();
                                       ++it)
          {
            histogramScratch[(*it).row]++;
          }
          for(std::list<lt_entry>::iterator it = thisWindow.begin();
                                       it != thisWindow.end();
                                       ++it)
          {
            (*it).mapCounter += histogramScratch[(*it).row];
          }
          return maxrow;
        }
        default:
          assert(0 && "Unknown LocalityTracker policy");
      }
      assert(0 && "Shouldn't get here");
      return 0;
    }

  private:
		std::string name;
    unsigned int *** openRow;
    std::list<lt_entry> ***window;
    unsigned int windowSize;
    lt_policy_t policy;
    uint64_t numAccesses;
    uint64_t numConflicts;
    unsigned int channels;
    unsigned int ranks;
    unsigned int banks;
    unsigned int rows;
    unsigned int roundRobinIndex;
    unsigned int *histogramScratch;
    unsigned int accessCounter;
    unsigned int lastController;
    unsigned int lastRank;
    unsigned int lastBank;
    unsigned int lastRow;
    uint64_t consecutiveRowHits;
};

#endif //#ifndef __LOCALITY_TRACKER_H__
