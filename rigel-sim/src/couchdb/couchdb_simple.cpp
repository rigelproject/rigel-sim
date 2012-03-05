#include "couchdb_simple.h"
#include <uuid/uuid.h>
#include <string>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

using namespace couchdb_simple;

std::string 
CouchDBSimple::GetRevNumberFromJSON(const std::string json)
{
  size_t pos_start = json.find("\"_rev\"");
  pos_start = json.find_first_of(":", pos_start);
  pos_start = json.find_first_of("\"", pos_start);
  size_t pos_end = json.find_first_of("\"", pos_start+1);
 
  if (std::string::npos == pos_start) { assert(0 && "Substring not found"); }
  if (std::string::npos == pos_end) { assert(0 && "Comma not found"); }

  return  json.substr(pos_start+1, pos_end-pos_start-1);
}

std::string 
CouchDBSimple::GetRevNumberStringFromJSON(const std::string json)
{
  size_t pos_start = json.find("\"_rev\"");
  size_t pos_end = json.find_first_of(",", pos_start+1);
 
  if (std::string::npos == pos_start) { assert(0 && "Substring not found"); }
  if (std::string::npos == pos_end) { assert(0 && "Comma not found"); }

  return  json.substr(pos_start, pos_end-pos_start);
}

std::string
CouchDBSimple::GetUUIDString() 
{
  uuid_t doc_name_uuid;
  uuid_generate(doc_name_uuid);

  const char uuid_convert_table[] = 
  {
    '0', '1', '2', '3', 
    '4', '5', '6', '7',
    '8', '9', 'a', 'b',
    'c', 'd', 'e', 'f'
  };

  char doc_name[33];
  for (int i = 0; i < 16; i++) {
    doc_name[2*i  ] = uuid_convert_table[0xF & doc_name_uuid[i]];
    doc_name[2*i+1] =  uuid_convert_table[0xF & (doc_name_uuid[i]>>4)];
  }
  doc_name[32] = '\0';
  return std::string(doc_name);
}

std::string
CouchDBSimple::GetRecord(
  const std::string db_name, 
  const std::string doc_name)
{
  // Setup a read request
  mReaderChunk.set_read_data("");
  // Set the URL and port.
  std::string url = db_hostname + "/" + db_name + "/" + doc_name;
  try
  {
    request.setOpt<Url>(url);
    request.setOpt<Port>(db_port);
    request.setOpt<InfileSize>(mReaderChunk.size());
    request.setOpt<Put>(false);
    // Send request to server.
    request.perform();
    // Dump output from server response.
    // TODO: Remove this print!
    //mWriterChunk.print();
    return mWriterChunk.get_data();
  }
  
  catch(curlpp::RuntimeError & e)
  {
    std::cerr << "CouchDBSimple::GetRecord: " << e.what() << std::endl;
  }
  catch(curlpp::LogicError & e)
  {
    std::cerr << "CouchDBSimple::GetRecord: " << e.what() << std::endl;
  }
  std::string snakeEyes("");
  return snakeEyes;
}

std::string
CouchDBSimple::CreateDB(const std::string db_name)
{
  // Setup a read request
  mReaderChunk.set_read_data("");

  // Set the URL and port.
  std::string url = db_hostname + "/" + db_name;
  try
  {
    request.setOpt<Url>(url);
    request.setOpt<Port>(db_port);
    request.setOpt<InfileSize>(mReaderChunk.size());
    request.setOpt<Put>(true);

    // Send request to server.
    request.perform();
    // Dump output from server response.
    return mWriterChunk.get_data();
  }

  catch(curlpp::RuntimeError & e)
  {
    std::cerr << "CouchDBSimple::CreateDB: " << e.what() << std::endl;
  }
  catch(curlpp::LogicError & e)
  {
    std::cerr << "CouchDBSimple::CreateDB: " << e.what() << std::endl;
  }
  std::string snakeEyes("");
  return snakeEyes;
}

std::string
CouchDBSimple::CreateRecord(
  const std::string db_name, 
  const std::string doc_name,
  const std::string record_json)
{
  // Setup a read request
  mReaderChunk.set_read_data("{" + record_json + "}");

  // Set the URL and port.
  std::string url = db_hostname + "/" + db_name + "/" + doc_name;
  try
  {
    request.setOpt<Url>(url);
    request.setOpt<Port>(db_port);
    request.setOpt<InfileSize>(mReaderChunk.size());
    request.setOpt<Put>(true);

    // Send request to server.
    request.perform();
    // Dump output from server response.
    return mWriterChunk.get_data();
  }

  catch(curlpp::RuntimeError & e)
  {
    std::cerr << "CouchDBSimple::CreateRecord: " << e.what() << std::endl;
  }
  catch(curlpp::LogicError & e)
  {
    std::cerr << "CouchDBSimple::CreateRecord: " << e.what() << std::endl;
  }
  std::string snakeEyes("");
  return snakeEyes;
}

std::string
CouchDBSimple::DeleteRecord(
  const std::string db_name, 
  const std::string doc_name)
{
  // TODO: This call does not seem to work.  I just get Conflict errors.
  if(GetRecord(db_name, doc_name).length() == 0) { std::string snakeEyes(""); return snakeEyes; }
  // Setup a read request
  mReaderChunk.set_read_data("");
  std::string rev_string = GetRevNumberFromJSON(mWriterChunk.get_data());

  // Set the URL and port.
  std::string url = 
    db_hostname + "/" + db_name + "/" + doc_name + "?rev=" + rev_string;
  try
  {
    request.setOpt<Url>(url);
    request.setOpt<Port>(db_port);
    request.setOpt<CustomRequest>("DELETE");
    // Send request to server.
    request.perform();
    // Dump output from server response.
    return mWriterChunk.get_data();
  }
  catch(curlpp::RuntimeError & e)
  {
    std::cerr << "CouchDBSimple::DeleteRecord: " << e.what() << std::endl;
  }
  catch(curlpp::LogicError & e)
  {
    std::cerr << "CouchDBSimple::DeleteRecord: " << e.what() << std::endl;
  }
  std::string snakeEyes("");
  return snakeEyes;
}

std::string
CouchDBSimple::UpdateRecord(
  const std::string db_name, 
  const std::string doc_name,
  const std::string record_json)
{
  // Grab the current record to extract the revision number.
  if(GetRecord(db_name, doc_name).length() == 0) { std::string snakeEyes(""); return snakeEyes; }
  std::string rev_string = GetRevNumberStringFromJSON(mWriterChunk.get_data());
  std::string read_data = "{ " + rev_string + "," + record_json + "}";
  // Set the URL and port.
  std::string url = db_hostname + "/" + db_name + "/" + doc_name;
  // Setup a read request
  mReaderChunk.set_read_data(read_data);
  try
  {
    // Setup the request.
    request.setOpt<Url>(url);
    request.setOpt<Port>(db_port);
    request.setOpt<InfileSize>(mReaderChunk.size());
    request.setOpt<Put>(true);
    // Send request to server.
    request.perform();
    // Dump output from server response.
    return mWriterChunk.get_data();
  }
  catch(curlpp::RuntimeError & e)
  {
    std::cerr << "CouchDBSimple::UpdateRecord: " << e.what() << std::endl;
  }
  catch(curlpp::LogicError & e)
  {
    std::cerr << "CouchDBSimple::UpdateRecord: " << e.what() << std::endl;
  }
  std::string snakeEyes("");
  return snakeEyes; 
}

std::string
CouchDBSimple::DeleteDB(
  const std::string db_name)
{
  // TODO: This call does not seem to work.  I just get Conflict errors.

  // Setup a read request
  mReaderChunk.set_read_data("");

  // Set the URL and port.
  std::string url = db_hostname + "/" + db_name;
  try
  {
    request.setOpt<Url>(url);
    request.setOpt<Port>(db_port);
    request.setOpt<CustomRequest>("DELETE");
    // Send request to server.
    request.perform();
    // Dump output from server response.
    return mWriterChunk.get_data();
  }
  catch(curlpp::RuntimeError & e)
  {
    std::cerr << "CouchDBSimple::DeleteDB: " << e.what() << std::endl;
  }
  catch(curlpp::LogicError & e)
  {
    std::cerr << "CouchDBSimple::DeleteDB: " << e.what() << std::endl;
  }
  std::string snakeEyes("");
  return snakeEyes; 
}

CouchDBSimple::CouchDBSimple(const std::string db_hostname, const size_t db_port) : 
  ENABLE_DEBUG(false),
  ENABLE_VERBOSE(false),
  mWriterChunk(),
  mReaderChunk(ENABLE_DEBUG),
  debugInfo(ENABLE_DEBUG),
  db_hostname(db_hostname), 
  db_port(db_port)
{
  using namespace curlpp::types;
  using namespace curlpp::options;

  // Standard options.
  request.setOpt<Verbose>(ENABLE_VERBOSE);

  // Setup a debug callback.
  request.setOpt(DebugFunction(DebugFunctionFunctor(&debugInfo, 
       &DebugInfoClass::writeDebug)));

  // Setup a read callback
  ReadFunctionFunctor 
    rFunctor(&mReaderChunk, &ReaderMemoryClass::ReadMemoryCallback);
  ReadFunction *readFunct = new ReadFunction(rFunctor);
  request.setOpt(readFunct);
  
  // Setup the writer callback.
  WriteFunctionFunctor 
    wFunctor(&mWriterChunk, &WriterMemoryClass::WriteMemoryCallback);
  curlpp::options::WriteFunction *writeFunct
      = new curlpp::options::WriteFunction(wFunctor);
  request.setOpt(writeFunct);
}
  
size_t 
ReaderMemoryClass::ReadMemoryCallback(char *buffer, size_t size, size_t nitems)
{
  if (ENABLE_DEBUG) {
    fprintf(stderr, "***READ*** size: %zu nitems: %zu content: '%s'\n", 
      size, nitems, read_data.c_str());
  }
  // Handle case where we have already completed.
  if (bytes_completed >= read_data.size()) return 0;
  // Calculate maximum bytes we can write.
  const size_t buf_size = size * nitems;
  const size_t remaining_bytes = read_data.size() - bytes_completed;
  const size_t bytes = remaining_bytes > buf_size ? buf_size : remaining_bytes;

  strncpy(buffer, read_data.c_str()+bytes_completed, bytes);
  bytes_completed += bytes;
  return bytes;
}
