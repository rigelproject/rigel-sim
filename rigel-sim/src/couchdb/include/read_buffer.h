#ifndef __READ_BUFFER_H__
#define __READ_BUFFER_H__


class ReaderMemoryClass
{
  public:
    // Constructor.
    ReaderMemoryClass();
    ReaderMemoryClass(bool debug_enabled) : 
      ENABLE_DEBUG(debug_enabled),
      read_data("{ }"),
      bytes_completed(0) { };
    // Destructor.
    ~ReaderMemoryClass() { };
    // Callback used by request.perform().
    size_t ReadMemoryCallback(char *buffer, size_t size, size_t nitems);
    // Total size of the request.
    size_t size() const { return read_data.size(); }
    // Initialize a new request.
    void set_read_data(const std::string s) { read_data = s; bytes_completed = 0; }

  private:
    bool ENABLE_DEBUG;
    std::string read_data;
    size_t bytes_completed;
};
    
#endif
