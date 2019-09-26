#ifndef RENDER_DEVICE_MANAGER
#define RENDER_DEVICE_MANAGER

#include "VulkanHeaders.hpp"

#include <vector>

class Renderer;
class RenderDeviceManager;

struct QueueProperties {
  QueueProperties(unsigned id, VkQueueFlags flags, bool presentation_support) :
    id(id), flags(flags), presentation_support(presentation_support) {}
  unsigned id;
  VkQueueFlags flags;
  bool presentation_support; 
};

struct SwapChainProperties {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

class RenderDevice {
public:
  RenderDevice(VkPhysicalDevice device, RenderDeviceManager* parent);
  ~RenderDevice();
  void Destroy();
  VkPhysicalDevice GetDevice();
  bool SupportsOperation(VkQueueFlagBits operation) const;
  int GetOperationQueueIndex(VkQueueFlagBits operation) const;
  int GetPresentQueueIndex() const;
  int GetMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
  bool DiscreteGPU() const;
  unsigned MaxTextureSize() const;
  bool SupportsRequiredExtensions(const std::vector<const char*>& required_extensions) const;
  const SwapChainProperties& GetSwapChainProperties() const;
  const VkSurfaceFormatKHR& GetPreferredSwapFormat(VkFormat format, VkColorSpaceKHR color_space) const;
  const VkPresentModeKHR GetPrefferedSwapMode(VkPresentModeKHR mode) const;
  void PrintSupportedOperations() const;

protected:
  const char* _device_name;
  VkPhysicalDevice _device;
  VkPhysicalDeviceProperties* _device_properties;
  std::vector<QueueProperties> _queue_proprties;
  std::vector<VkExtensionProperties> _device_extensions;
  SwapChainProperties _swapchain_properties;
  RenderDeviceManager* _parent;
};

class RenderDeviceManager {
public:
  RenderDeviceManager(Renderer* parent) : _parent(parent) {}
  bool InitDevices();
  RenderDevice* GetCurrentDevice();
  int GetOperationQueueIndex(VkQueueFlagBits operation) const;
  int GetPresentQueueIndex() const;
  Renderer* GetRenderer();

protected:
  unsigned RateDevice(const RenderDevice& device) const;
  RenderDevice* GuessBestDevice();
  RenderDevice* _current_device = nullptr;
  std::vector<RenderDevice> _devices;
  Renderer* _parent;
};

#endif
