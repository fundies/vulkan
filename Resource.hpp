#ifndef RESOURCE_HPP
#define RESOURCE_HPP

#include <vector>

class Resource {
public:
  Resource(const char* start, const char* end);
  const std::vector<char>& data() const; 

private:
  std::vector<char> _data;
};

#define LOAD_RESOURCE(x) ([]() {                                      \
  extern const char _binary_##x##_start, _binary_##x##_end;           \
  return Resource(&_binary_##x##_start, &_binary_##x##_end);          \
})()

#endif
