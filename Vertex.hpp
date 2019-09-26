#include "VulkanHeaders.hpp"

#include <array>

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  static VkVertexInputBindingDescription GetBindingDescription();
  static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions();
};
