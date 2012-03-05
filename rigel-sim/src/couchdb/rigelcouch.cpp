#include <iostream>
#include <string>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "couchdb_simple.h"

const std::string db_hostname("http://stout");
const uint32_t db_port = 5984;

using namespace curlpp::options;

static void
usage(const char *exe_name) 
{
  fprintf(stderr, "Usage: %s <action> <db_name> <db record>\n", exe_name);
  fprintf(stderr, "  Allowed actions:\n");
  fprintf(stderr, "\t(u)pdate a record\n");
  fprintf(stderr, "\t(c)reate a record\n");
  fprintf(stderr, "\t(d)elete a record\n");
  fprintf(stderr, "\t(C)reate a DB\n");
  fprintf(stderr, "\t(D)elete a DB\n");
  fprintf(stderr, "\t(g)et a record\n");
}

int
main(int argc, char ** argv)
{
  // Handle command line arguments.
  if (argc < 3) {
    usage(argv[0]);
    exit(1);
  }
  const std::string db_name(argv[2]);
  const std::string doc_name(argc == 4 ? argv[3] : "");
  const std::string record_data("\"foo\": \"bar\"");
 
  curlpp::Cleanup myCleanup;
  // Initialize the couchDB interface object.
  couchdb_simple::CouchDBSimple couchObj(db_hostname, db_port);

  // Parse the action.
  switch (argv[1][0]) 
  {
    case 'C': couchObj.CreateDB(db_name); break;
    case 'D': couchObj.DeleteDB(db_name); break;
    case 'c': couchObj.CreateRecord(db_name, doc_name, record_data); break;
    case 'u': couchObj.UpdateRecord(db_name, doc_name, record_data); break;
    case 'd': couchObj.DeleteRecord(db_name, doc_name); break;
    case 'g': std::cerr << "===WRITE=== " << couchObj.GetRecord(db_name, doc_name) << endl;; break;
    default: 
      usage(argv[0]); 
      exit(1); 
  };

  return 0;
}
