#include "Resource.hpp"

Resource::Resource(const char *start, const char *end): _data(start, start+(end - start)) {}
const std::vector<char>& Resource::data() const {
  return _data;
}
