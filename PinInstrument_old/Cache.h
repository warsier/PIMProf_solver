//===- Cache.h - Cache implementation ---------------------------*- C++ -*-===//
//
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef __CACHE_H__
#define __CACHE_H__

#include <string>
#include <list>
#include <vector>
#include <set>

#include "PinUtil.h"
#include "pin.H"

/// @brief Checks if n is a power of 2.
/// @returns true if n is power of 2
static inline bool IsPower2(UINT32 n)
{
    return ((n & (n - 1)) == 0);
}

/// @brief Computes floor(log2(n))
/// Works by finding position of MSB set.
/// @returns -1 if n == 0.
static inline INT32 FloorLog2(UINT32 n)
{
    INT32 p = 0;

    if (n == 0)
        return -1;

    if (n & 0xffff0000)
    {
        p += 16;
        n >>= 16;
    }
    if (n & 0x0000ff00)
    {
        p += 8;
        n >>= 8;
    }
    if (n & 0x000000f0)
    {
        p += 4;
        n >>= 4;
    }
    if (n & 0x0000000c)
    {
        p += 2;
        n >>= 2;
    }
    if (n & 0x00000002)
    {
        p += 1;
    }

    return p;
}

/// @brief Computes ceil(log2(n))
/// Works by finding position of MSB set.
/// @returns -1 if n == 0.
static inline INT32 CeilLog2(UINT32 n)
{
    return FloorLog2(n - 1) + 1;
}


namespace PIMProf {


/// @brief Cache tag
/// dynamic data structure should only be allocated on construction
/// and deleted on destruction
class CACHE_TAG
{
  private:
    ADDRINT _tag;
    DataReuseSegment _seg;

  public:
    CACHE_TAG(ADDRINT tagaddr = 0)
    {
        _tag = tagaddr;
    }

    inline bool operator == (const ADDRINT &rhs) const { return _tag == rhs; }

    inline VOID SetTag(ADDRINT tagaddr) {
       _tag = tagaddr;
    }
    inline ADDRINT GetTag() const { return _tag; }

    inline VOID InsertOnHit(BBLID bblid, ACCESS_TYPE accessType);

    inline VOID SplitOnMiss();
};


class CACHE_SET
{
  protected:
    static const UINT32 MAX_ASSOCIATIVITY = 32;
  public:
    virtual ~CACHE_SET() {};
    virtual VOID SetAssociativity(UINT32 associativity) = 0;
    virtual UINT32 GetAssociativity(UINT32 associativity) = 0;
    virtual CACHE_TAG *Find(ADDRINT tagaddr) = 0;
    virtual CACHE_TAG *Replace(ADDRINT tagaddr) = 0;
    virtual VOID Flush() = 0;
};


class DIRECT_MAPPED : public CACHE_SET
{
  private:
    CACHE_TAG *_tag;

  public:
    inline DIRECT_MAPPED(UINT32 associativity = 1) 
    {
        ASSERTX(associativity == 1);
        _tag = new CACHE_TAG(0);
    }

    inline ~DIRECT_MAPPED()
    {
        delete _tag;
    }

    inline VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
    inline UINT32 GetAssociativity(UINT32 associativity) { return 1; }

    inline CACHE_TAG *Find(ADDRINT tagaddr) 
    {
        if (*_tag == tagaddr) {
            return _tag;
        }
        return NULL;
    }
    inline CACHE_TAG *Replace(ADDRINT tagaddr) 
    {
        _tag->SetTag(tagaddr);
        return _tag;
    }
    inline VOID Flush() { _tag->SetTag(0); }
};

/// @brief Cache set with round robin replacement
class ROUND_ROBIN : public CACHE_SET
{
  private:
    CACHE_TAG *_tags[MAX_ASSOCIATIVITY];
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

  public:
    
    inline ROUND_ROBIN(UINT32 associativity)
        : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index] = new CACHE_TAG(0);
        }
    }

    inline ~ROUND_ROBIN()
    {
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            delete _tags[index];
        }
    }

    inline VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;
    }

    inline UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

    inline CACHE_TAG *Find(ADDRINT tagaddr)
    {
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            if (*_tags[index] == tagaddr)
                return _tags[index];
        }
        return NULL;
    }

    inline CACHE_TAG *Replace(ADDRINT tagaddr)
    {
        // g++ -O3 too dumb to do CSE on following lines?!
        const UINT32 index = _nextReplaceIndex;

        _tags[index]->SetTag(tagaddr);
        // condition typically faster than modulo
        _nextReplaceIndex = (index == 0 ? _tagsLastIndex : index - 1);
        return _tags[index];
    }

    inline VOID Flush()
    {
        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index]->SetTag(0);
        }
        _nextReplaceIndex = _tagsLastIndex;
    }
};

class LRU : public CACHE_SET
{
  public:
    typedef std::list<CACHE_TAG *> CacheTagList;

  private:
    // this is a fixed-size list where the size is the current associativity
    // front is MRU, back is LRU
    CacheTagList _tags;

  public:
    inline LRU(UINT32 associativity = MAX_ASSOCIATIVITY)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        for (UINT32 i = 0; i < associativity; i++)
        {
            CACHE_TAG *tag = new CACHE_TAG(0);
            tag->SetTag(0);
            _tags.push_back(tag);
        }
    }

    inline ~LRU()
    {
        while (!_tags.empty())
        {
            CACHE_TAG *tag = _tags.back();
            delete tag;
            _tags.pop_back();
        }
    }

    inline VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        while (!_tags.empty())
        {
            CACHE_TAG *tag = _tags.back();
            delete tag;
            _tags.pop_back();
        }
        for (UINT32 i = 0; i < associativity; i++)
        {
            CACHE_TAG *tag = new CACHE_TAG(0);
            _tags.push_back(tag);
        }
    }

    inline UINT32 GetAssociativity(UINT32 associativity)
    {
        return _tags.size();
    }

    inline CACHE_TAG *Find(ADDRINT tagaddr)
    {
        CacheTagList::iterator it = _tags.begin();
        CacheTagList::iterator eit = _tags.end();
        for (; it != eit; it++)
        {
            CACHE_TAG *tag = *it;
            // promote the accessed cache line to the front
            if (*tag == tagaddr)
            {
                _tags.erase(it);
                _tags.push_front(tag);
                return _tags.front();
            }
        }
        return NULL;
    }

    inline CACHE_TAG *Replace(ADDRINT tagaddr)
    {
        CACHE_TAG *tag = _tags.back();
        _tags.pop_back();
        tag->SetTag(tagaddr);
        _tags.push_front(tag);
        return tag;
    }

    inline VOID Flush()
    {
        UINT32 associativity = _tags.size();
        while (!_tags.empty())
        {
            CACHE_TAG *tag = _tags.back();
            delete tag;
            _tags.pop_back();
        }
        for (UINT32 i = 0; i < associativity; i++)
        {
            CACHE_TAG *tag = new CACHE_TAG(0);
            _tags.push_back(tag);
        }
    }
};


namespace CACHE_ALLOC
{
enum STORE_ALLOCATION
{
    STORE_ALLOCATE,
    STORE_NO_ALLOCATE
};
}

/// @brief Generic cache base class; no allocate specialization, no cache set specialization
class CACHE_LEVEL_BASE
{
  protected:
    static const UINT32 HIT_MISS_NUM = 2;
    CACHE_STATS _access[ACCESS_TYPE_NUM][HIT_MISS_NUM];

  protected:
    // input params
    const std::string _name;
    const UINT32 _cacheSize;
    const UINT32 _lineSize;
    const UINT32 _associativity;
    UINT32 _numberOfFlushes;
    UINT32 _numberOfResets;

    // computed params
    const UINT32 _lineShift;
    const UINT32 _setIndexMask;

    CACHE_STATS SumAccess(bool hit) const
    {
        CACHE_STATS sum = 0;

        for (UINT32 accessType = 0; accessType < ACCESS_TYPE_NUM; accessType++)
        {
            sum += _access[accessType][hit];
        }

        return sum;
    }

  protected:
    UINT32 NumSets() const { return _setIndexMask + 1; }

  public:
    // constructors/destructors
    CACHE_LEVEL_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity);

    // accessors
    UINT32 CacheSize() const { return _cacheSize; }
    UINT32 LineSize() const { return _lineSize; }
    UINT32 Associativity() const { return _associativity; }
    //
    CACHE_STATS Hits(ACCESS_TYPE accessType) const { return _access[accessType][true]; }
    CACHE_STATS Misses(ACCESS_TYPE accessType) const { return _access[accessType][false]; }
    CACHE_STATS Accesses(ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType); }
    CACHE_STATS Hits() const { return SumAccess(true); }
    CACHE_STATS Misses() const { return SumAccess(false); }
    CACHE_STATS Accesses() const { return Hits() + Misses(); }

    CACHE_STATS Flushes() const { return _numberOfFlushes; }
    CACHE_STATS Resets() const { return _numberOfResets; }

    VOID SplitAddress(const ADDRINT addr, ADDRINT &tagaddr, UINT32 &setIndex) const
    {
        tagaddr = addr >> _lineShift;
        setIndex = tagaddr & _setIndexMask;
    }

    VOID SplitAddress(const ADDRINT addr, ADDRINT &tagaddr, UINT32 &setIndex, UINT32 &lineIndex) const
    {
        const UINT32 lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SplitAddress(addr, tagaddr, setIndex);
    }

    VOID IncFlushCounter()
    {
        _numberOfFlushes += 1;
    }

    VOID IncResetCounter()
    {
        _numberOfResets += 1;
    }

    /// @brief Stats output method
    std::ostream &StatsLong(std::ostream &out) const;
    VOID CountMemoryCost(std::vector<COST> (&_BBL_cost)[MAX_COST_SITE], int cache_level) const;

    
};


/// @brief Templated cache class with specific cache set allocation policies
/// All that remains to be done here is allocate and deallocate the right
/// type of cache sets.
class CACHE_LEVEL : public CACHE_LEVEL_BASE
{
  private:
    std::string _replacement_policy;
    std::vector<CACHE_SET *> _sets;
    UINT32 STORE_ALLOCATION;
    COST _hitcost[MAX_COST_SITE];
  
  // forbid copy constructor
  private:
    CACHE_LEVEL(const CACHE_LEVEL &);

  public:
    // constructors/destructors
    CACHE_LEVEL(std::string name, std::string policy, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity, UINT32 allocation, COST hitcost[MAX_COST_SITE]);
    ~CACHE_LEVEL();

    // modifiers
    
    VOID AddMemCost(BOOL hit, CACHE_LEVEL *lvl);

    /// Cache access from addr to addr+size-1/*!
    /// @return true if all accessed cache lines hit
    BOOL Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Cache access at addr that does not span cache lines
    /// @return true if accessed cache line hits
    BOOL AccessSingleLine(ADDRINT addr, ACCESS_TYPE accessType);

    VOID Flush();
    VOID ResetStats();
    inline std::string getReplacementPolicy() {
        return _replacement_policy;
    }
};

class CACHE 
{
  friend class CostSolver;
  public:
    static const UINT32 MAX_LEVEL = 6;
    enum {
        ITLB, DTLB, IL1, DL1, UL2, UL3
    };
    static const std::string _name[MAX_LEVEL];
  private:
    static CACHE_LEVEL *_cache[MAX_LEVEL];

  // forbid copy constructor
  private:
    CACHE(const CACHE &);

  public:
    CACHE();
    CACHE(const std::string filename);
    ~CACHE();

    VOID ReadConfig(std::string filename);

    /// Write the current cache config to ofstream or file.
    /// If no modification is made, then this will output the 
    /// default cache config PIMProf will use.
    std::ostream& WriteConfig(std::ostream& out);
    VOID WriteConfig(const std::string filename);

    std::ostream& WriteStats(std::ostream& out);
    VOID WriteStats(const std::string filename);

    VOID Ul2Access(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Do on instruction cache reference
    VOID InstrCacheRef(ADDRINT addr);

    /// Do on multi-line data cache references
    VOID DataCacheRefMulti(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);

    /// Do on a single-line data cache reference
    VOID DataCacheRefSingle(ADDRINT addr, UINT32 size, ACCESS_TYPE accessType);
};

std::string StringInt(UINT64 val, UINT32 width = 0, CHAR padding = ' ');
std::string StringHex(UINT64 val, UINT32 width = 0, CHAR padding = ' ');
std::string StringString(std::string val, UINT32 width = 0, CHAR padding = ' ');

} // namespace PIMProf

#endif // __CACHE_H__