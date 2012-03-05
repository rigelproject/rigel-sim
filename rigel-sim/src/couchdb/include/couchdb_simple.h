#ifndef __COUCHDB_SIMPLE_H__
#define __COUCHDB_SIMPLE_H__

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

namespace couchdb_simple
{
  using namespace curlpp::options;
  class DebugInfoClass
  {
    public:
      DebugInfoClass();
      DebugInfoClass(bool debug_enabled) : ENABLE_DEBUG(debug_enabled) { }
      int writeDebug(curl_infotype, char *data, size_t size)
      {
        // Early out when debugging disabled.
        if (!ENABLE_DEBUG) { return size; }
  
        fprintf(stderr, "[DEBUG]: ");
        fprintf(stderr, "%s ", data);
        
        return size;
      }
    private:
      const bool ENABLE_DEBUG;
  };
    
  #include "write_buffer.h"
  #include "read_buffer.h"

  class CouchDBSimple
  {
    public:
      // Default constructor diabled.
      CouchDBSimple();
      // Constructor.
      CouchDBSimple(const std::string db_hostname, const size_t db_port);

    private: /* METHODS */
      // Return just the revision number from a CouchDB JSON record.
      std::string GetRevNumberFromJSON(const std::string json);
      // Return just the whole "_rev":"REV" string from a CouchDB JSON record.
      std::string GetRevNumberStringFromJSON(const std::string json);

    public: /* METHODS */
      // Use the WriterMemoryClass to grab a record from the DB.
      std::string GetRecord(const std::string db_name, const std::string doc_name);
      // Add a new DB to the CouchDB server.
      std::string CreateDB(const std::string db_name);
      // Create a new record in the DB.
      std::string CreateRecord(const std::string db_name, 
        const std::string doc_name, const std::string record_json);
      // Delete last record in the DB.
      std::string DeleteRecord(const std::string db_name, const std::string doc_name);
      // Update a record in the DB at the latest rev.
      std::string UpdateRecord(const std::string db_name, 
        const std::string doc_name, const std::string record_json);
      // Delete an entire DB.
      std::string DeleteDB(const std::string db_name);
      // Helper for generating a UUID as a 32-charachter hex string.
      static std::string GetUUIDString();

    private: /* DATA */
      // Debugging enable switch.
      const bool ENABLE_DEBUG;
      // Verbose output enable switch.
      const bool ENABLE_VERBOSE;
      // Persistent writer object used for GET callbacks.
      WriterMemoryClass mWriterChunk;
      // Persistent reader object used for PUT/POST callbacks.
      ReaderMemoryClass mReaderChunk;
      // Persistent debug object.
      DebugInfoClass debugInfo;
      // Our request to be sent.
      curlpp::Easy request;
      // Hostname for the CouchDB server. 
      const std::string db_hostname;
      // Port for the CouchDB server. 
      const size_t db_port;
  };
};


#endif
