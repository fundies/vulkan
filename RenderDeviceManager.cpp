#include "RenderDeviceManager.hpp"
#include "Renderer.hpp"

#include <iostream>
#include <cstring>
#include <map>
#include <algorithm>

RenderDevice::RenderDevice(VkPhysicalDevice device, RenderDeviceManager* parent) : _device(device), _parent(parent) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, queueFamilies.data());

  auto surface = _parent->GetRenderer()->GetVKSurface();

  int i = 0;
  for (const auto& queueFamily : queueFamilies) {
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, surface, &presentSupport);
    _queue_proprties.emplace_back(i++, queueFamily.queueFlags, presentSupport);
  }

  _device_properties = new VkPhysicalDeviceProperties;
  vkGetPhysicalDeviceProperties(_device, _device_properties);

  _device_name = _device_properties->deviceName;

  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, nullptr);
  _device_extensions.resize(extensionCount);
  vkEnumerateDeviceExtensionProperties(_device, nullptr, &extensionCount, _device_extensions.data());

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device, surface, &_swapchain_properties.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(_device, surface, &formatCount, nullptr);

  if (formatCount != 0) {
    _swapchain_properties.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_device, surface, &formatCount, _swapchain_properties.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(_device, surface, &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    _swapchain_properties.present_modes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(_device, surface, &presentModeCount, _swapchain_properties.present_modes.data());
  }

  PrintSupportedOperations();
}

RenderDevice::~RenderDevice() {
  delete _device_properties;
}

VkPhysicalDevice RenderDevice::GetDevice() {
  return _device;
}

bool RenderDevice::SupportsOperation(VkQueueFlagBits operation) const {
  for (const auto &p : _queue_proprties) 
    if (p.flags & operation) 
      return true;

  return false;
}

int RenderDevice::GetOperationQueueIndex(VkQueueFlagBits operation) const {
  for (const auto &p : _queue_proprties) 
    if (p.flags & operation) 
      return p.id;

  return -1;
}

int RenderDevice::GetPresentQueueIndex() const {
  // if our Graphics queue can also present use that
  int q = GetOperationQueueIndex(VK_QUEUE_GRAPHICS_BIT);
  if (q != -1)
    if (_queue_proprties[q].presentation_support) return q;

  for (const auto &p : _queue_proprties) 
    if (p.presentation_support) 
      return p.id;
  
  return -1;
}

int RenderDevice::GetMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(_device, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  return -1;
}

bool RenderDevice::DiscreteGPU() const {
  return (_device_properties->deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
}

unsigned RenderDevice::MaxTextureSize() const {
  return _device_properties->limits.maxImageDimension2D;
}

bool RenderDevice::SupportsRequiredExtensions(const std::vector<const char*>& required_extensions) const {
  for (const auto &re : required_extensions) {
    for (const auto &de : _device_extensions) {
      const char* ext = de.extensionName;
      if (strcmp(re, ext) == 0) break;
      if (&de == &(*_device_extensions.rbegin())) {
        std::cerr << "Device does not support required extension: \"" << re << "\"" << std::endl;
        return false;
      }
    }
  }
  
  return true;
}

const SwapChainProperties& RenderDevice::GetSwapChainProperties() const {
  return _swapchain_properties;
}

const VkSurfaceFormatKHR& RenderDevice::GetPreferredSwapFormat(VkFormat format, VkColorSpaceKHR color_space) const {
  auto it = std::find_if(_swapchain_properties.formats.begin(), _swapchain_properties.formats.end(), [&format, &color_space](const VkSurfaceFormatKHR &p) {
    return (p.format == format && p.colorSpace == color_space);
  });

  if (it != _swapchain_properties.formats.end()) {
   return *it;
  } else {
   std::cerr << "Requested swap format not supported" << std::endl; 
   return _swapchain_properties.formats.front();
  }
}

const VkPresentModeKHR RenderDevice::GetPrefferedSwapMode(VkPresentModeKHR mode) const {
  auto it = std::find(_swapchain_properties.present_modes.begin(), _swapchain_properties.present_modes.end(), mode);
  if (it != _swapchain_properties.present_modes.end()) return *it;
  else {
    std::cerr << "Requested present mode not supported" << std::endl; 
    return VK_PRESENT_MODE_FIFO_KHR;
  }
}

void RenderDevice::PrintSupportedOperations() const {
  std::cout << "[" << _device_name << "]" << std::endl << "\tDevice Queues: " <<  _queue_proprties.size() << std::endl;
  
  for (const auto &p : _queue_proprties) { 
    std::cout << "\t\tQueue " << p.id << ": " << std::endl;
    std::cout << "\t\t\t";
    if (p.flags & VK_QUEUE_GRAPHICS_BIT)
      std::cout << "VK_QUEUE_GRAPHICS_BIT ";
    if (p.flags & VK_QUEUE_COMPUTE_BIT)
      std::cout << "VK_QUEUE_COMPUTE_BIT ";
    if (p.flags & VK_QUEUE_TRANSFER_BIT)
      std::cout << "VK_QUEUE_TRANSFER_BIT ";
    if (p.flags & VK_QUEUE_SPARSE_BINDING_BIT)
      std::cout << "VK_QUEUE_SPARSE_BINDING_BIT ";
    if (p.flags & VK_QUEUE_PROTECTED_BIT)
      std::cout << "VK_QUEUE_PROTECTED_BIT ";
    std::cout << std::endl;
    std::cout << "\t\t\t" << "Supports presentation: " << (p.presentation_support ? "true" : "false") << std::endl;
  }

  std::cout << std::endl << "\tSupported Extensions: " << _device_extensions.size() << std::endl;
  std::cout << "\t\t";
  for (const auto& e : _device_extensions) {
    std::cout << e.extensionName << " ";
  }

  std::cout << std::endl;

  const std::map<VkFormat, const char*> swap_formats = {
    { VK_FORMAT_R4G4_UNORM_PACK8, "VK_FORMAT_R4G4_UNORM_PACK8" },
    { VK_FORMAT_R4G4B4A4_UNORM_PACK16, "VK_FORMAT_R4G4B4A4_UNORM_PACK16" },
    { VK_FORMAT_B4G4R4A4_UNORM_PACK16, "VK_FORMAT_B4G4R4A4_UNORM_PACK16" },
    { VK_FORMAT_R5G6B5_UNORM_PACK16, "VK_FORMAT_R5G6B5_UNORM_PACK16" },
    { VK_FORMAT_B5G6R5_UNORM_PACK16, "VK_FORMAT_B5G6R5_UNORM_PACK16" },
    { VK_FORMAT_R5G5B5A1_UNORM_PACK16, "VK_FORMAT_R5G5B5A1_UNORM_PACK16" },
    { VK_FORMAT_B5G5R5A1_UNORM_PACK16, "VK_FORMAT_B5G5R5A1_UNORM_PACK16" },
    { VK_FORMAT_A1R5G5B5_UNORM_PACK16, "VK_FORMAT_A1R5G5B5_UNORM_PACK16" },
    { VK_FORMAT_R8_UNORM, "VK_FORMAT_R8_UNORM" },
    { VK_FORMAT_R8_SNORM, "VK_FORMAT_R8_SNORM" },
    { VK_FORMAT_R8_USCALED, "VK_FORMAT_R8_USCALED" },
    { VK_FORMAT_R8_SSCALED, "VK_FORMAT_R8_SSCALED" },
    { VK_FORMAT_R8_UINT, "VK_FORMAT_R8_UINT" },
    { VK_FORMAT_R8_SINT, "VK_FORMAT_R8_SINT" },
    { VK_FORMAT_R8_SRGB, "VK_FORMAT_R8_SRGB" },
    { VK_FORMAT_R8G8_UNORM, "VK_FORMAT_R8G8_UNORM" },
    { VK_FORMAT_R8G8_SNORM, "VK_FORMAT_R8G8_SNORM" },
    { VK_FORMAT_R8G8_USCALED, "VK_FORMAT_R8G8_USCALED" },
    { VK_FORMAT_R8G8_SSCALED, "VK_FORMAT_R8G8_SSCALED" },
    { VK_FORMAT_R8G8_UINT, "VK_FORMAT_R8G8_UINT" },
    { VK_FORMAT_R8G8_SINT, "VK_FORMAT_R8G8_SINT" },
    { VK_FORMAT_R8G8_SRGB, "VK_FORMAT_R8G8_SRGB" }, 
    { VK_FORMAT_R8G8B8_UNORM, "VK_FORMAT_R8G8B8_UNORM" },
    { VK_FORMAT_R8G8B8_SNORM, "VK_FORMAT_R8G8B8_SNORM" },
    { VK_FORMAT_R8G8B8_USCALED, "VK_FORMAT_R8G8B8_USCALED" },
    { VK_FORMAT_R8G8B8_SSCALED, "VK_FORMAT_R8G8B8_SSCALED" },
    { VK_FORMAT_R8G8B8_UINT, "VK_FORMAT_R8G8B8_UINT" },
    { VK_FORMAT_R8G8B8_SINT, "VK_FORMAT_R8G8B8_SINT" },
    { VK_FORMAT_R8G8B8_SRGB, "VK_FORMAT_R8G8B8_SRGB" },
    { VK_FORMAT_B8G8R8_UNORM, "VK_FORMAT_B8G8R8_UNORM" },
    { VK_FORMAT_B8G8R8_SNORM, "VK_FORMAT_B8G8R8_SNORM" },
    { VK_FORMAT_B8G8R8_USCALED, "VK_FORMAT_B8G8R8_USCALED" },
    { VK_FORMAT_B8G8R8_SSCALED, "VK_FORMAT_B8G8R8_SSCALED" },
    { VK_FORMAT_B8G8R8_UINT, "VK_FORMAT_B8G8R8_UINT" },
    { VK_FORMAT_B8G8R8_SINT, "VK_FORMAT_B8G8R8_SINT" },
    { VK_FORMAT_B8G8R8_SRGB, "VK_FORMAT_B8G8R8_SRGB" },
    { VK_FORMAT_R8G8B8A8_UNORM, "VK_FORMAT_R8G8B8A8_UNORM" },
    { VK_FORMAT_R8G8B8A8_SNORM, "VK_FORMAT_R8G8B8A8_SNORM" },
    { VK_FORMAT_R8G8B8A8_USCALED, "VK_FORMAT_R8G8B8A8_USCALED" },
    { VK_FORMAT_R8G8B8A8_SSCALED, "VK_FORMAT_R8G8B8A8_SSCALED" },
    { VK_FORMAT_R8G8B8A8_UINT, "VK_FORMAT_R8G8B8A8_UINT" },
    { VK_FORMAT_R8G8B8A8_SINT, "VK_FORMAT_R8G8B8A8_SINT" },
    { VK_FORMAT_R8G8B8A8_SRGB, "VK_FORMAT_R8G8B8A8_SRGB" },
    { VK_FORMAT_B8G8R8A8_UNORM, "VK_FORMAT_B8G8R8A8_UNORM" },
    { VK_FORMAT_B8G8R8A8_SNORM, "VK_FORMAT_B8G8R8A8_SNORM" },
    { VK_FORMAT_B8G8R8A8_USCALED, "VK_FORMAT_B8G8R8A8_USCALED" },
    { VK_FORMAT_B8G8R8A8_SSCALED, "VK_FORMAT_B8G8R8A8_SSCALED" },
    { VK_FORMAT_B8G8R8A8_UINT, "VK_FORMAT_B8G8R8A8_UINT" },
    { VK_FORMAT_B8G8R8A8_SINT, "VK_FORMAT_B8G8R8A8_SINT" },
    { VK_FORMAT_B8G8R8A8_SRGB, "VK_FORMAT_B8G8R8A8_SRGB" },
    { VK_FORMAT_A8B8G8R8_UNORM_PACK32, "VK_FORMAT_A8B8G8R8_UNORM_PACK32" },
    { VK_FORMAT_A8B8G8R8_SNORM_PACK32, "VK_FORMAT_A8B8G8R8_SNORM_PACK32" },
    { VK_FORMAT_A8B8G8R8_USCALED_PACK32, "VK_FORMAT_A8B8G8R8_USCALED_PACK32" },
    { VK_FORMAT_A8B8G8R8_SSCALED_PACK32, "VK_FORMAT_A8B8G8R8_SSCALED_PACK32" },
    { VK_FORMAT_A8B8G8R8_UINT_PACK32, "VK_FORMAT_A8B8G8R8_UINT_PACK32" },
    { VK_FORMAT_A8B8G8R8_SINT_PACK32, "VK_FORMAT_A8B8G8R8_SINT_PACK32" },
    { VK_FORMAT_A8B8G8R8_SRGB_PACK32, "VK_FORMAT_A8B8G8R8_SRGB_PACK32" },
    { VK_FORMAT_A2R10G10B10_UNORM_PACK32, "VK_FORMAT_A2R10G10B10_UNORM_PACK32" },
    { VK_FORMAT_A2R10G10B10_SNORM_PACK32, "VK_FORMAT_A2R10G10B10_SNORM_PACK32" },
    { VK_FORMAT_A2R10G10B10_USCALED_PACK32, "VK_FORMAT_A2R10G10B10_USCALED_PACK32" },
    { VK_FORMAT_A2R10G10B10_SSCALED_PACK32, "VK_FORMAT_A2R10G10B10_SSCALED_PACK32" },
    { VK_FORMAT_A2R10G10B10_UINT_PACK32, "VK_FORMAT_A2R10G10B10_UINT_PACK32" },
    { VK_FORMAT_A2R10G10B10_SINT_PACK32, "VK_FORMAT_A2R10G10B10_SINT_PACK32" },
    { VK_FORMAT_A2B10G10R10_UNORM_PACK32, "VK_FORMAT_A2B10G10R10_UNORM_PACK32" },
    { VK_FORMAT_A2B10G10R10_SNORM_PACK32, "VK_FORMAT_A2B10G10R10_SNORM_PACK32" },
    { VK_FORMAT_A2B10G10R10_USCALED_PACK32, "VK_FORMAT_A2B10G10R10_USCALED_PACK32" },
    { VK_FORMAT_A2B10G10R10_SSCALED_PACK32, "VK_FORMAT_A2B10G10R10_SSCALED_PACK32" },
    { VK_FORMAT_A2B10G10R10_UINT_PACK32, "VK_FORMAT_A2B10G10R10_UINT_PACK32" },
    { VK_FORMAT_A2B10G10R10_SINT_PACK32, "VK_FORMAT_A2B10G10R10_SINT_PACK32" },
    { VK_FORMAT_R16_UNORM, "VK_FORMAT_R16_UNORM" },
    { VK_FORMAT_R16_SNORM, "VK_FORMAT_R16_SNORM" },
    { VK_FORMAT_R16_USCALED, "VK_FORMAT_R16_USCALED" },
    { VK_FORMAT_R16_SSCALED, "VK_FORMAT_R16_SSCALED" },
    { VK_FORMAT_R16_UINT, "VK_FORMAT_R16_UINT" },
    { VK_FORMAT_R16_SINT, "VK_FORMAT_R16_SINT" },
    { VK_FORMAT_R16_SFLOAT, "VK_FORMAT_R16_SFLOAT" },
    { VK_FORMAT_R16G16_UNORM, "VK_FORMAT_R16G16_UNORM" },
    { VK_FORMAT_R16G16_SNORM, "VK_FORMAT_R16G16_SNORM" },
    { VK_FORMAT_R16G16_USCALED, "VK_FORMAT_R16G16_USCALED" },
    { VK_FORMAT_R16G16_SSCALED, "VK_FORMAT_R16G16_SSCALED" },
    { VK_FORMAT_R16G16_UINT, "VK_FORMAT_R16G16_UINT" },
    { VK_FORMAT_R16G16_SINT, "VK_FORMAT_R16G16_SINT" },
    { VK_FORMAT_R16G16_SFLOAT, "VK_FORMAT_R16G16_SFLOAT" },
    { VK_FORMAT_R16G16B16_UNORM, "VK_FORMAT_R16G16B16_UNORM" },
    { VK_FORMAT_R16G16B16_SNORM, "VK_FORMAT_R16G16B16_SNORM" },
    { VK_FORMAT_R16G16B16_USCALED, "VK_FORMAT_R16G16B16_USCALED" },
    { VK_FORMAT_R16G16B16_SSCALED, "VK_FORMAT_R16G16B16_SSCALED" },
    { VK_FORMAT_R16G16B16_UINT, "VK_FORMAT_R16G16B16_UINT" },
    { VK_FORMAT_R16G16B16_SINT, "VK_FORMAT_R16G16B16_SINT" },
    { VK_FORMAT_R16G16B16_SFLOAT, "VK_FORMAT_R16G16B16_SFLOAT" },
    { VK_FORMAT_R16G16B16A16_UNORM, "VK_FORMAT_R16G16B16A16_UNORM" },
    { VK_FORMAT_R16G16B16A16_SNORM, "VK_FORMAT_R16G16B16A16_SNORM" },
    { VK_FORMAT_R16G16B16A16_USCALED, "VK_FORMAT_R16G16B16A16_USCALED" },
    { VK_FORMAT_R16G16B16A16_SSCALED, "VK_FORMAT_R16G16B16A16_SSCALED" },
    { VK_FORMAT_R16G16B16A16_UINT, "VK_FORMAT_R16G16B16A16_UINT" },
    { VK_FORMAT_R16G16B16A16_SINT, "VK_FORMAT_R16G16B16A16_SINT" },
    { VK_FORMAT_R16G16B16A16_SFLOAT, "VK_FORMAT_R16G16B16A16_SFLOAT" },
    { VK_FORMAT_R32_UINT, "VK_FORMAT_R32_UINT" },
    { VK_FORMAT_R32_SINT, "VK_FORMAT_R32_SINT" },
    { VK_FORMAT_R32_SFLOAT, "VK_FORMAT_R32_SFLOAT" },
    { VK_FORMAT_R32G32_UINT, "VK_FORMAT_R32G32_UINT" },
    { VK_FORMAT_R32G32_SINT, "VK_FORMAT_R32G32_SINT" },
    { VK_FORMAT_R32G32_SFLOAT, "VK_FORMAT_R32G32_SFLOAT" },
    { VK_FORMAT_R32G32B32_UINT, "VK_FORMAT_R32G32B32_UINT" },
    { VK_FORMAT_R32G32B32_SINT, "VK_FORMAT_R32G32B32_SINT" },
    { VK_FORMAT_R32G32B32_SFLOAT, "VK_FORMAT_R32G32B32_SFLOAT" },
    { VK_FORMAT_R32G32B32A32_UINT, "VK_FORMAT_R32G32B32A32_UINT" },
    { VK_FORMAT_R32G32B32A32_SINT, "VK_FORMAT_R32G32B32A32_SINT" },
    { VK_FORMAT_R32G32B32A32_SFLOAT, "VK_FORMAT_R32G32B32A32_SFLOAT" },
    { VK_FORMAT_R64_UINT, "VK_FORMAT_R64_UINT" },
    { VK_FORMAT_R64_SINT, "VK_FORMAT_R64_SINT" },
    { VK_FORMAT_R64_SFLOAT, "VK_FORMAT_R64_SFLOAT" },
    { VK_FORMAT_R64G64_UINT, "VK_FORMAT_R64G64_UINT" },
    { VK_FORMAT_R64G64_SINT, "VK_FORMAT_R64G64_SINT" },
    { VK_FORMAT_R64G64_SFLOAT, "VK_FORMAT_R64G64_SFLOAT" },
    { VK_FORMAT_R64G64B64_UINT, "VK_FORMAT_R64G64B64_UINT" },
    { VK_FORMAT_R64G64B64_SINT, "VK_FORMAT_R64G64B64_SINT" },
    { VK_FORMAT_R64G64B64_SFLOAT, "VK_FORMAT_R64G64B64_SFLOAT" },
    { VK_FORMAT_R64G64B64A64_UINT, "VK_FORMAT_R64G64B64A64_UINT" },
    { VK_FORMAT_R64G64B64A64_SINT, "VK_FORMAT_R64G64B64A64_SINT" },
    { VK_FORMAT_R64G64B64A64_SFLOAT, "VK_FORMAT_R64G64B64A64_SFLOAT" },
    { VK_FORMAT_B10G11R11_UFLOAT_PACK32, "VK_FORMAT_B10G11R11_UFLOAT_PACK32" },
    { VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32" },
    { VK_FORMAT_D16_UNORM, "VK_FORMAT_D16_UNORM" },
    { VK_FORMAT_X8_D24_UNORM_PACK32, "VK_FORMAT_X8_D24_UNORM_PACK32" },
    { VK_FORMAT_D32_SFLOAT, "VK_FORMAT_D32_SFLOAT" },
    { VK_FORMAT_S8_UINT, "VK_FORMAT_S8_UINT" },
    { VK_FORMAT_D16_UNORM_S8_UINT, "VK_FORMAT_D16_UNORM_S8_UINT" },
    { VK_FORMAT_D24_UNORM_S8_UINT, "VK_FORMAT_D24_UNORM_S8_UINT" },
    { VK_FORMAT_D32_SFLOAT_S8_UINT, "VK_FORMAT_D32_SFLOAT_S8_UINT" },
    { VK_FORMAT_BC1_RGB_UNORM_BLOCK, "VK_FORMAT_BC1_RGB_UNORM_BLOCK" },
    { VK_FORMAT_BC1_RGB_SRGB_BLOCK, "VK_FORMAT_BC1_RGB_SRGB_BLOCK" },
    { VK_FORMAT_BC1_RGBA_UNORM_BLOCK, "VK_FORMAT_BC1_RGBA_UNORM_BLOCK" },
    { VK_FORMAT_BC1_RGBA_SRGB_BLOCK, "VK_FORMAT_BC1_RGBA_SRGB_BLOCK" },
    { VK_FORMAT_BC2_UNORM_BLOCK, "VK_FORMAT_BC2_UNORM_BLOCK" },
    { VK_FORMAT_BC2_SRGB_BLOCK, "VK_FORMAT_BC2_SRGB_BLOCK" },
    { VK_FORMAT_BC3_UNORM_BLOCK, "VK_FORMAT_BC3_UNORM_BLOCK" },
    { VK_FORMAT_BC3_SRGB_BLOCK, "VK_FORMAT_BC3_SRGB_BLOCK" },
    { VK_FORMAT_BC4_UNORM_BLOCK, "VK_FORMAT_BC4_UNORM_BLOCK" },
    { VK_FORMAT_BC4_SNORM_BLOCK, "VK_FORMAT_BC4_SNORM_BLOCK" },
    { VK_FORMAT_BC5_UNORM_BLOCK, "VK_FORMAT_BC5_UNORM_BLOCK" },
    { VK_FORMAT_BC5_SNORM_BLOCK, "VK_FORMAT_BC5_SNORM_BLOCK" },
    { VK_FORMAT_BC6H_UFLOAT_BLOCK, "VK_FORMAT_BC6H_UFLOAT_BLOCK" },
    { VK_FORMAT_BC6H_SFLOAT_BLOCK, "VK_FORMAT_BC6H_SFLOAT_BLOCK" },
    { VK_FORMAT_BC7_UNORM_BLOCK, "VK_FORMAT_BC7_UNORM_BLOCK" },
    { VK_FORMAT_BC7_SRGB_BLOCK, "VK_FORMAT_BC7_SRGB_BLOCK" },
    { VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK, "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK" },
    { VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK, "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK" },
    { VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK, "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK" },
    { VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK, "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK" },
    { VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK, "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK" }, 
    { VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK, "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK" },
    { VK_FORMAT_EAC_R11_UNORM_BLOCK, "VK_FORMAT_EAC_R11_UNORM_BLOCK" },
    { VK_FORMAT_EAC_R11_SNORM_BLOCK, "VK_FORMAT_EAC_R11_SNORM_BLOCK" },
    { VK_FORMAT_EAC_R11G11_UNORM_BLOCK, "VK_FORMAT_EAC_R11G11_UNORM_BLOCK" },
    { VK_FORMAT_EAC_R11G11_SNORM_BLOCK, "VK_FORMAT_EAC_R11G11_SNORM_BLOCK" },
    { VK_FORMAT_ASTC_4x4_UNORM_BLOCK, "VK_FORMAT_ASTC_4x4_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_4x4_SRGB_BLOCK, "VK_FORMAT_ASTC_4x4_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_5x4_UNORM_BLOCK, "VK_FORMAT_ASTC_5x4_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_5x4_SRGB_BLOCK, "VK_FORMAT_ASTC_5x4_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_5x5_UNORM_BLOCK, "VK_FORMAT_ASTC_5x5_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_5x5_SRGB_BLOCK, "VK_FORMAT_ASTC_5x5_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_6x5_UNORM_BLOCK, "VK_FORMAT_ASTC_6x5_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_6x5_SRGB_BLOCK, "VK_FORMAT_ASTC_6x5_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_6x6_UNORM_BLOCK, "VK_FORMAT_ASTC_6x6_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_6x6_SRGB_BLOCK, "VK_FORMAT_ASTC_6x6_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_8x5_UNORM_BLOCK, "VK_FORMAT_ASTC_8x5_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_8x5_SRGB_BLOCK, "VK_FORMAT_ASTC_8x5_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_8x6_UNORM_BLOCK, "VK_FORMAT_ASTC_8x6_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_8x6_SRGB_BLOCK, "VK_FORMAT_ASTC_8x6_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_8x8_UNORM_BLOCK, "VK_FORMAT_ASTC_8x8_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_8x8_SRGB_BLOCK, "VK_FORMAT_ASTC_8x8_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_10x5_UNORM_BLOCK, "VK_FORMAT_ASTC_10x5_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_10x5_SRGB_BLOCK, "VK_FORMAT_ASTC_10x5_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_10x6_UNORM_BLOCK, "VK_FORMAT_ASTC_10x6_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_10x6_SRGB_BLOCK, "VK_FORMAT_ASTC_10x6_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_10x8_UNORM_BLOCK, "VK_FORMAT_ASTC_10x8_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_10x8_SRGB_BLOCK, "VK_FORMAT_ASTC_10x8_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_10x10_UNORM_BLOCK, "VK_FORMAT_ASTC_10x10_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_10x10_SRGB_BLOCK, "VK_FORMAT_ASTC_10x10_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_12x10_UNORM_BLOCK, "VK_FORMAT_ASTC_12x10_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_12x10_SRGB_BLOCK, "VK_FORMAT_ASTC_12x10_SRGB_BLOCK" },
    { VK_FORMAT_ASTC_12x12_UNORM_BLOCK, "VK_FORMAT_ASTC_12x12_UNORM_BLOCK" },
    { VK_FORMAT_ASTC_12x12_SRGB_BLOCK, "VK_FORMAT_ASTC_12x12_SRGB_BLOCK" },
    { VK_FORMAT_G8B8G8R8_422_UNORM, "VK_FORMAT_G8B8G8R8_422_UNORM" },
    { VK_FORMAT_B8G8R8G8_422_UNORM, "VK_FORMAT_B8G8R8G8_422_UNORM" },
    { VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM, "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM" },
    { VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM" },
    { VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM, "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM" },
    { VK_FORMAT_G8_B8R8_2PLANE_422_UNORM, "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM" },
    { VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM, "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM" },
    { VK_FORMAT_R10X6_UNORM_PACK16, "VK_FORMAT_R10X6_UNORM_PACK16" },
    { VK_FORMAT_R10X6G10X6_UNORM_2PACK16, "VK_FORMAT_R10X6G10X6_UNORM_2PACK16" },
    { VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16, "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16" },
    { VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16, "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16" },
    { VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16, "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16" },
    { VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16" },
    { VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16" },
    { VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16" },
    { VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16, "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16" }, 
    { VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16, "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16" },
    { VK_FORMAT_R12X4_UNORM_PACK16, "VK_FORMAT_R12X4_UNORM_PACK16" },
    { VK_FORMAT_R12X4G12X4_UNORM_2PACK16, "VK_FORMAT_R12X4G12X4_UNORM_2PACK16" },
    { VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16, "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16" },
    { VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16, "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16" },
    { VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16, "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16" },
    { VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16" },
    { VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16, "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16" },
    { VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16" },
    { VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16, "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16" },
    { VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16, "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16" },
    { VK_FORMAT_G16B16G16R16_422_UNORM, "VK_FORMAT_G16B16G16R16_422_UNORM" },
    { VK_FORMAT_B16G16R16G16_422_UNORM, "VK_FORMAT_B16G16R16G16_422_UNORM" },
    { VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM, "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM" },
    { VK_FORMAT_G16_B16R16_2PLANE_420_UNORM, "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM" },
    { VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM, "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM" },
    { VK_FORMAT_G16_B16R16_2PLANE_422_UNORM, "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM" },
    { VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM, "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM" },
    { VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG, "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG" },
    { VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG, "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG" },
    { VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG, "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG" },
    { VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG, "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG" }, 
    { VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG, "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG" },
    { VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG, "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG" },
    { VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG, "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG" },
    { VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG, "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG" },
    { VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT" },
    { VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT, "VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT" }
  };

  const std::map<VkColorSpaceKHR, const char*> color_spaces = {
    { VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR" },
    { VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT, "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT" },
    { VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT" },
    { VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT, "VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT VK_COLOR_SPACE_DCI_P3_LINEAR_EXT" },
    { VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT, "VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT" },
    { VK_COLOR_SPACE_BT709_LINEAR_EXT, "VK_COLOR_SPACE_BT709_LINEAR_EXT" },
    { VK_COLOR_SPACE_BT709_NONLINEAR_EXT, "VK_COLOR_SPACE_BT709_NONLINEAR_EXT" },
    { VK_COLOR_SPACE_BT2020_LINEAR_EXT, "VK_COLOR_SPACE_BT2020_LINEAR_EXT" },
    { VK_COLOR_SPACE_HDR10_ST2084_EXT, "VK_COLOR_SPACE_HDR10_ST2084_EXT" },
    { VK_COLOR_SPACE_DOLBYVISION_EXT, "VK_COLOR_SPACE_DOLBYVISION_EXT" },
    { VK_COLOR_SPACE_HDR10_HLG_EXT, "VK_COLOR_SPACE_HDR10_HLG_EXT" },
    { VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT, "VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT" },
    { VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT, "VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT" },
    { VK_COLOR_SPACE_PASS_THROUGH_EXT, "VK_COLOR_SPACE_PASS_THROUGH_EXT" },
    { VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT, "VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT" },
    { VK_COLOR_SPACE_DISPLAY_NATIVE_AMD, "K_COLOR_SPACE_DISPLAY_NATIVE_AMD" }
  };

  std::cout << std::endl << "\tSupported Swap Formats: " << _swapchain_properties.formats.size() << std::endl;
  for (const auto& f : _swapchain_properties.formats) {
    std::cout << "\t\t" << swap_formats.at(f.format) << ":" << std::endl;
    std::cout << "\t\t\tSupported color spaces: " << color_spaces.at(f.colorSpace) << std::endl;
  }

  std::cout << std::endl;

  const std::map<VkPresentModeKHR, const char*> present_modes = {
    { VK_PRESENT_MODE_IMMEDIATE_KHR, "VK_PRESENT_MODE_IMMEDIATE_KHR" },
    { VK_PRESENT_MODE_MAILBOX_KHR, "VK_PRESENT_MODE_MAILBOX_KHR" },
    { VK_PRESENT_MODE_FIFO_KHR, "VK_PRESENT_MODE_FIFO_KHR" },
    { VK_PRESENT_MODE_FIFO_RELAXED_KHR, "VK_PRESENT_MODE_FIFO_RELAXED_KHR" },
    { VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR, "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR" },
    { VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR, "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR" }
  };

  std::cout << std::endl << "\tSupported Presentation Modes: " << _swapchain_properties.present_modes.size() << std::endl;
  std::cout << "\t\t";
  for (const auto& f : _swapchain_properties.present_modes) {
    std::cout << present_modes.at(f) << " ";
  }

  std::cout << std::endl;
}

bool RenderDeviceManager::InitDevices() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(_parent->GetVKInstance(), &deviceCount, nullptr);

  if (deviceCount == 0) {
    std::cerr << "Failed to find any GPUs with Vulkan support" << std::endl;
    return false;
  }

  std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
  vkEnumeratePhysicalDevices(_parent->GetVKInstance(), &deviceCount, physicalDevices.data());
  for (uint32_t i = 0; i < deviceCount; ++i) {
    _devices.emplace_back(physicalDevices[i], this);
  }

  _current_device = GuessBestDevice();

  if (_current_device == nullptr) {
    std::cerr << "Failed to find any compatible GPUs" << std::endl;
    return false;
  }

  return true;
}

unsigned RenderDeviceManager::RateDevice(const RenderDevice& device) const {
  unsigned score = 0;

  // if device cant draw is useless
  if (!device.SupportsOperation(VK_QUEUE_GRAPHICS_BIT) || device.GetPresentQueueIndex() == -1)
    return 0;

  if (!device.SupportsRequiredExtensions(_parent->GetRequiredExtensions()))
    return 0;

  // Discrete GPUs have a significant performance advantage
  if (device.DiscreteGPU())
      score += 1000;

  // Maximum possible size of textures affects graphics quality
  score += device.MaxTextureSize();

  return score;
}

RenderDevice* RenderDeviceManager::GuessBestDevice() {
  unsigned maxscore = 0;
  RenderDevice* bestDevice = nullptr;
  for (auto& device : _devices) {
    if (unsigned currentscore = RateDevice(device) > maxscore) {
      maxscore = currentscore;
      bestDevice = &device;
    }
  }

  return bestDevice;
}

RenderDevice* RenderDeviceManager::GetCurrentDevice() {
  return _current_device;
}

int RenderDeviceManager::GetOperationQueueIndex(VkQueueFlagBits operation) const {
  return _current_device->GetOperationQueueIndex(operation);
}

int RenderDeviceManager::GetPresentQueueIndex() const {
  return _current_device->GetPresentQueueIndex();
}

Renderer* RenderDeviceManager::GetRenderer() {
  return _parent;
}
