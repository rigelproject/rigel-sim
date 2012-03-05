#ifndef __KVMAP_H__
#define __KVMAP_H__

#include <string>
#include <map>

class KVMap
{
  typedef std::map<std::string, std::string> KVType;
  public:
    KVMap() { }
    bool insert(std::string k, std::string v) { std::pair<std::string,std::string> a(k, v); return m.insert(a).second; }
    std::string serialize() {
      std::string ret;
      KVType::const_iterator it = m.begin(), end = m.end();
      ret += "\""; ret += it->first; ret += "\": "; ret += it->second; ++it;
      for(; it != end; ++it)
      {
        ret += ", "; ret += "\""; ret += it->first; ret += "\": "; ret += it->second;
      }
      return ret;
    }
  private:
    std::map<std::string, std::string> m;
};

#endif //#ifndef __KVMAP_H__
