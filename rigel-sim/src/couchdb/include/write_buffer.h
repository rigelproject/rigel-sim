#ifndef __WRITE_BUFFER_H__
#define __WRITE_BUFFER_H__
#include <string.h>

#define MAX_FILE_LENGTH 20000
 
class WriterMemoryClass
{
  public:
    // Helper Class for reading result from remote host
    WriterMemoryClass()
    {
      this->m_pBuffer = NULL;
      this->m_pBuffer = (char*) malloc(MAX_FILE_LENGTH * sizeof(char));
      this->m_Size = 0;
    };
   
    ~WriterMemoryClass()
    {
      if (this->m_pBuffer) { free(this->m_pBuffer); }
    };
   
    void* Realloc(void* ptr, size_t size)
    {
      return ptr ?realloc(ptr, size) : malloc(size);
    };
   
    // Callback must be declared static, otherwise it won't link...
    size_t WriteMemoryCallback(char* ptr, size_t size, size_t nmemb)
    {
      // Calculate the real size of the incoming buffer
      size_t realsize = size * nmemb;
   
      // (Re)Allocate memory for the buffer
      m_pBuffer = (char*) Realloc(m_pBuffer, m_Size + realsize);
   
      // Test if Buffer is initialized correctly & copy memory
      if (m_pBuffer == NULL) {
        realsize = 0;
      }
   
      memcpy(&(m_pBuffer[m_Size]), ptr, realsize);
      m_Size += realsize;
   
   
      // return the real size of the buffer...
      return realsize;
    };
   
    void print() 
    {
      std::cerr << "===WRITE=== size: " << m_Size << " content: ";
      std::cerr << m_pBuffer << std::endl;
    }
    // Return a string containing the data that the server returned.
    std::string get_data() const { return std::string(m_pBuffer); }
   
    // Public member vars
    char* m_pBuffer;
    size_t m_Size;
  };

#endif
