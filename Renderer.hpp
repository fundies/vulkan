#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "RenderDeviceManager.hpp"

#include <vector>

class Renderer {
public:
  Renderer();
  ~Renderer();
  bool Init(GLFWwindow* window, const char* game_name, const char* engine_name, const std::vector<const char*>& extensions);
  bool DebugEnabled();
  VkInstance GetVKInstance();
  VkSurfaceKHR GetVKSurface();
  const std::vector<const char*> GetRequiredExtensions() const;
  void DrawFrame();
  
  bool framebuffer_resized = false;

protected:
  bool InitInstance(const char* game_name, const char* engine_name, const std::vector<const char*>& extensions);
  bool InitLogicalDevice();
  bool InitSurface();
  bool InitSwapChain();
  void DestroySwapChain();
  bool ResetSwapChain();
  bool InitGraphicsPipeline();
  bool InitShader(VkShaderModule& shader_module, const std::vector<char>& code);
  bool InitDescriptorSetLayout();
  bool InitDescriptorSets();
  bool InitDescriptorPool();
  bool InitRenderPass();
  bool InitFramebuffers();
  bool InitCommandPool();
  bool InitTextureImage();
  bool InitTextureImageView();
  bool InitTextureSampler();
  bool InitImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
  bool InitImageView(VkImageView& imageView, VkImage image, VkFormat format);
  bool InitImageViews();
  bool TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
  bool InitCommandBuffers();
  bool InitSyncObjects();
  bool InitVertexBuffer();
  bool InitIndexBuffer();
  bool InitBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
  void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
  void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
  bool InitUniformBuffers();
  void UpdateUniformBuffer(uint32_t currentImage);
  VkCommandBuffer BeginSingleTimeCommands();
  void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
  bool HasValidationSupport();
  void PrintExtensions();
  
  GLFWwindow* _window;
  VkInstance _vk_instance;
  VkDevice _vk_logical_device;
  VkQueue _vk_graphics_queue; //these two queues are likely the same but could be different
  VkQueue _vk_present_queue;
  VkSurfaceKHR _vk_surface;
  VkSwapchainKHR _vk_swapchain;
  VkExtent2D _vk_swapchain_extent;
  VkFormat _vk_swapchain_image_format;
  std::vector<VkImage> _vk_swapchain_images;
  std::vector<VkImageView> _vk_swapchain_image_views;
  VkRenderPass _vk_render_pass;
  VkDescriptorSetLayout _vk_descriptor_set_layout;
  std::vector<VkFramebuffer> _vk_swapchain_framebuffers;
  VkPipelineLayout _vk_pipeline_layout;
  VkPipeline _vk_graphics_pipeline;
  VkCommandPool _vk_command_pool;
  VkImage _vk_texture_image;
  VkDeviceMemory _vk_texture_image_memory;
  VkImageView _vk_texture_image_view;
  VkSampler _vk_texture_sampler;
  VkBuffer _vk_vertex_buffer;
  VkDeviceMemory _vk_vertex_buffer_memory;
  VkBuffer _vk_index_buffer;
  VkDeviceMemory _vk_index_buffer_memory;
  std::vector<VkBuffer> _vk_uniform_buffers;
  std::vector<VkDeviceMemory> _vk_uniform_buffers_memory;
  VkDescriptorPool _vk_descriptor_pool;
  std::vector<VkDescriptorSet> _vk_descriptor_sets;
  std::vector<VkCommandBuffer> _vk_command_buffers;
  std::vector<VkSemaphore> _vk_image_available_semaphores;
  std::vector<VkSemaphore> _vk_render_finished_semaphores;
  std::vector<VkFence> _vk_in_flight_fences;
  size_t _current_frame = 0;
  VkDebugUtilsMessengerEXT _vk_debug_messenger;
  std::vector<VkExtensionProperties> _vk_extensions;
  const std::vector<const char*> _vk_validation_layers = std::vector<const char*>({ "VK_LAYER_KHRONOS_validation" });
  const std::vector<const char*> _vk_required_extenstions = std::vector<const char*>({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });
  std::vector<VkLayerProperties> _vk_layer_properties;
  RenderDeviceManager _device_manager;
  
  #ifdef NDEBUG
  const bool _enable_validation_layers = false;
  #else
  const bool _enable_validation_layers = true;
  #endif
};

#endif
