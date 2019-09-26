#include "Renderer.hpp"
#include "Resource.hpp"
#include "Vertex.hpp"

#include <iostream>
#include <algorithm>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

static const int MAX_FRAMES_IN_FLIGHT = 2;

struct UniformBufferObject {
  glm::mat4 view;
  glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
  {{0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
  {{1280.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
  {{1280.0f, 720.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
  {{0.0f, 720.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
  0, 1, 2, 2, 3, 0
};


static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
   const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, 
    const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, 
    void* pUserData) {
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
  return VK_FALSE;
}

Renderer::Renderer() : _device_manager(this) {
}

Renderer::~Renderer() {

  vkDeviceWaitIdle(_vk_logical_device);

  DestroySwapChain();

  vkDestroySampler(_vk_logical_device, _vk_texture_sampler, nullptr);
  vkDestroyImageView(_vk_logical_device, _vk_texture_image_view, nullptr);

  vkDestroyImage(_vk_logical_device, _vk_texture_image, nullptr);
  vkFreeMemory(_vk_logical_device, _vk_texture_image_memory, nullptr);

  vkDestroyDescriptorSetLayout(_vk_logical_device, _vk_descriptor_set_layout, nullptr);

  vkDestroyBuffer(_vk_logical_device, _vk_index_buffer, nullptr);
  vkFreeMemory(_vk_logical_device, _vk_index_buffer_memory, nullptr);

  vkDestroyBuffer(_vk_logical_device, _vk_vertex_buffer, nullptr);
  vkFreeMemory(_vk_logical_device, _vk_vertex_buffer_memory, nullptr);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(_vk_logical_device, _vk_render_finished_semaphores[i], nullptr);
    vkDestroySemaphore(_vk_logical_device, _vk_image_available_semaphores[i], nullptr);
    vkDestroyFence(_vk_logical_device, _vk_in_flight_fences[i], nullptr);
  }

  vkDestroyCommandPool(_vk_logical_device, _vk_command_pool, nullptr);

  vkDestroyDevice(_vk_logical_device, nullptr);
  
  if (DebugEnabled()) {
    DestroyDebugUtilsMessengerEXT(_vk_instance, _vk_debug_messenger, nullptr);
  }
  
  vkDestroySurfaceKHR(_vk_instance, _vk_surface, nullptr);
  vkDestroyInstance(_vk_instance, nullptr);
}

bool Renderer::Init(GLFWwindow* window, const char* game_name, const char* engine_name, const std::vector<const char*>& extensions) {
  _window = window;

  if (
       (!InitInstance(game_name, engine_name, extensions))
    || (!InitSurface())
    || (!_device_manager.InitDevices())
    || (!InitLogicalDevice())
    || (!InitSwapChain())
    || (!InitImageViews())
    || (!InitRenderPass())
    || (!InitDescriptorSetLayout())
    || (!InitGraphicsPipeline())
    || (!InitFramebuffers())
    || (!InitCommandPool())
    || (!InitTextureImage())
    || (!InitTextureImageView())
    || (!InitTextureSampler())
    || (!InitVertexBuffer())
    || (!InitIndexBuffer())
    || (!InitUniformBuffers())
    || (!InitDescriptorPool())
    || (!InitDescriptorSets())
    || (!InitCommandBuffers())
    || (!InitSyncObjects())
   ) {

    std::cerr << "Failed to initialize vulkan" << std::endl;
    return false;
  }
  return true;
}

bool Renderer::DebugEnabled() {
  return _enable_validation_layers;
}

bool Renderer::InitInstance(const char* game_name, const char* engine_name, const std::vector<const char*>& extensions) {
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = game_name;
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = engine_name;
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  createInfo.enabledExtensionCount = extensions.size();
  createInfo.ppEnabledExtensionNames = extensions.data();

  createInfo.enabledLayerCount = 0;

  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  _vk_extensions.resize(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, _vk_extensions.data());

  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  _vk_layer_properties.resize(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, _vk_layer_properties.data());

  // Validation Layers
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
  if (DebugEnabled() && !HasValidationSupport()) {
    std::cerr << "Vulkan validation layers requested, but not available" << std::endl;
  } else if (DebugEnabled()) {
    createInfo.enabledLayerCount = _vk_validation_layers.size();
    createInfo.ppEnabledLayerNames = _vk_validation_layers.data();

    debugCreateInfo = {};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = DebugCallback;
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;

  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  if (vkCreateInstance(&createInfo, nullptr, &_vk_instance) != VK_SUCCESS) {
    std::cerr << "Failed to initialize vulkan instance" << std::endl;
    return false;
  }

  if (DebugEnabled()) {
       if (CreateDebugUtilsMessengerEXT(_vk_instance, &debugCreateInfo, nullptr, &_vk_debug_messenger) != VK_SUCCESS) {
      std::cerr << "Failed to set up vulkan debug messenger" << std::endl;
      return false;
    }

    std::cout << "Vulkan validation layers enabled" << std::endl;
  }

  return true;
}

bool Renderer::HasValidationSupport() {
  auto it = std::find_if(_vk_layer_properties.begin(), _vk_layer_properties.end(), [this](const auto& i) {
    return (strcmp(_vk_validation_layers[0], i.layerName) == 0);
  });
  return (it != _vk_layer_properties.end());
}

void Renderer::PrintExtensions() {
  for (const auto& ext : _vk_extensions) std::cout << "\t" << ext.extensionName << std::endl;
}

VkInstance Renderer::GetVKInstance() {
  return _vk_instance;
}

VkSurfaceKHR Renderer::GetVKSurface() {
  return _vk_surface;
}

bool Renderer::InitLogicalDevice() {

  int graphicsQueueID = _device_manager.GetOperationQueueIndex(VK_QUEUE_GRAPHICS_BIT);
  int presentsQueueID = _device_manager.GetPresentQueueIndex();

  if (graphicsQueueID == -1 || presentsQueueID == -1) {
    std::cerr << "Failed to find suitable graphics queue(s)" << std::endl;
    return false;
  } 
  
  // If graphics queues are seperate we need two devices
  unsigned queueCount = (graphicsQueueID != presentsQueueID) + 1;
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(queueCount);

  float queuePriority = 1.0f;
  for (unsigned i = 0; i < queueCount; ++i) {
    VkDeviceQueueCreateInfo& queueCreateInfo = queueCreateInfos[i];
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = ((i == 0) ? graphicsQueueID : presentsQueueID);
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
  }

  VkPhysicalDeviceFeatures deviceFeatures = {};
  deviceFeatures.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = queueCreateInfos.size();

  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount = _vk_required_extenstions.size();
  createInfo.ppEnabledExtensionNames = _vk_required_extenstions.data();

  if (_enable_validation_layers) {
    createInfo.enabledLayerCount = _vk_validation_layers.size();
    createInfo.ppEnabledLayerNames = _vk_validation_layers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(_device_manager.GetCurrentDevice()->GetDevice(), &createInfo, nullptr, &_vk_logical_device) 
    != VK_SUCCESS) {
    std::cerr << "Failed to create logical vulkan device" << std::endl;
    return false;
  }

  vkGetDeviceQueue(_vk_logical_device, queueCreateInfos[0].queueFamilyIndex, 0, &_vk_graphics_queue);

  if (queueCount == 1)
    _vk_present_queue = _vk_graphics_queue;
  else
    vkGetDeviceQueue(_vk_logical_device, queueCreateInfos[1].queueFamilyIndex, 0, &_vk_present_queue);

  return true;
}

bool Renderer::InitSurface() {
  if (glfwCreateWindowSurface(_vk_instance, _window, nullptr, &_vk_surface) != VK_SUCCESS) {
    std::cerr << "Failed to create window surface for vulkan" << std::endl;
    return false; 
  }
  
  return true;
}

bool Renderer::InitSwapChain() {
  RenderDevice* device = _device_manager.GetCurrentDevice();
  SwapChainProperties swapchain_props = device->GetSwapChainProperties();

  VkSurfaceFormatKHR surfaceFormat = device->GetPreferredSwapFormat(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
  VkPresentModeKHR presentMode = device->GetPrefferedSwapMode(VK_PRESENT_MODE_MAILBOX_KHR);

  int width, height;
  glfwGetWindowSize(_window, &width, &height);
  
  VkExtent2D extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

  uint32_t imageCount;
  if (swapchain_props.capabilities.maxImageCount > 0) 
    imageCount = std::min(swapchain_props.capabilities.minImageCount + 1, swapchain_props.capabilities.maxImageCount);
  else imageCount = swapchain_props.capabilities.minImageCount + 1;

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = _vk_surface;

  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  int graphicsQueueID = _device_manager.GetOperationQueueIndex(VK_QUEUE_GRAPHICS_BIT);
  int presentsQueueID = _device_manager.GetPresentQueueIndex();

  uint32_t queues[2] = { static_cast<uint32_t>(graphicsQueueID), static_cast<uint32_t>(presentsQueueID) };

  if (graphicsQueueID != presentsQueueID) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queues;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = swapchain_props.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(_vk_logical_device, &createInfo, nullptr, &_vk_swapchain) != VK_SUCCESS) {
    std::cerr << "Failed to create swap chain" << std::endl;
    return false;
  }

  vkGetSwapchainImagesKHR(_vk_logical_device, _vk_swapchain, &imageCount, nullptr);
  _vk_swapchain_images.resize(imageCount);
  vkGetSwapchainImagesKHR(_vk_logical_device, _vk_swapchain, &imageCount, _vk_swapchain_images.data());

  _vk_swapchain_image_format = surfaceFormat.format;
  _vk_swapchain_extent = extent;

  return true;
}

void Renderer::DestroySwapChain() {

  vkDeviceWaitIdle(_vk_logical_device);

  for (auto framebuffer : _vk_swapchain_framebuffers) {
    vkDestroyFramebuffer(_vk_logical_device, framebuffer, nullptr);
  }

  vkDestroyPipeline(_vk_logical_device, _vk_graphics_pipeline, nullptr);
  vkDestroyPipelineLayout(_vk_logical_device, _vk_pipeline_layout, nullptr);
  vkDestroyRenderPass(_vk_logical_device, _vk_render_pass, nullptr);

  for (auto imageView : _vk_swapchain_image_views) {
    vkDestroyImageView(_vk_logical_device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(_vk_logical_device, _vk_swapchain, nullptr);

  for (size_t i = 0; i < _vk_swapchain_images.size(); i++) {
    vkDestroyBuffer(_vk_logical_device, _vk_uniform_buffers[i], nullptr);
    vkFreeMemory(_vk_logical_device, _vk_uniform_buffers_memory[i], nullptr);
  }

  vkDestroyDescriptorPool(_vk_logical_device, _vk_descriptor_pool, nullptr);
}

bool Renderer::ResetSwapChain() {
  vkDeviceWaitIdle(_vk_logical_device);

  DestroySwapChain();

  if (
     !InitSwapChain()
  || !InitImageViews()
  || !InitRenderPass()
  || !InitGraphicsPipeline()
  || !InitFramebuffers()
  || !InitUniformBuffers()
  || !InitDescriptorPool()
  || !InitDescriptorSets()
  || !InitCommandBuffers()
  ) { return false; }

  return true;
}

bool Renderer::InitGraphicsPipeline() {
  auto vertShaderCode = LOAD_RESOURCE(default_vert_spv).data();
  auto fragShaderCode = LOAD_RESOURCE(default_frag_spv).data();

  VkShaderModule vertShaderModule;
  VkShaderModule fragShaderModule;

  if (!InitShader(vertShaderModule, vertShaderCode) || !InitShader(fragShaderModule, fragShaderCode)) {
    vkDestroyShaderModule(_vk_logical_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(_vk_logical_device, vertShaderModule, nullptr);
    return false;
  }

  VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
  vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
  fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  auto bindingDescription = Vertex::GetBindingDescription();
  auto attributeDescriptions = Vertex::GetAttributeDescriptions();

  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(_vk_swapchain_extent.width);
  viewport.height = static_cast<float>(_vk_swapchain_extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = _vk_swapchain_extent;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &_vk_descriptor_set_layout;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  bool ret = true;
  if (vkCreatePipelineLayout(_vk_logical_device, &pipelineLayoutInfo, nullptr, &_vk_pipeline_layout) != VK_SUCCESS) {
    std::cerr << "Failed to create pipeline layout" << std::endl;
    ret = false;
  }

  if (ret) {
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = _vk_pipeline_layout;
    pipelineInfo.renderPass = _vk_render_pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(_vk_logical_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_vk_graphics_pipeline) != VK_SUCCESS) {
     std::cerr << "Failed to create graphics pipeline" << std::endl;
     ret = false;
    }
  }

  vkDestroyShaderModule(_vk_logical_device, fragShaderModule, nullptr);
  vkDestroyShaderModule(_vk_logical_device, vertShaderModule, nullptr);

  return ret;
}

bool Renderer::InitShader(VkShaderModule& shader_module, const std::vector<char>& code) {
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  if (vkCreateShaderModule(_vk_logical_device, &createInfo, nullptr, &shader_module) != VK_SUCCESS) {
    std::cerr << "Failed to create shader module" << std::endl;
    return false;
  }

  return true;
}

bool Renderer::InitDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding uboLayoutBinding = {};
  uboLayoutBinding.binding = 0;
  uboLayoutBinding.descriptorCount = 1;
  uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  uboLayoutBinding.pImmutableSamplers = nullptr;
  uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
  samplerLayoutBinding.binding = 1;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = nullptr;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(_vk_logical_device, &layoutInfo, nullptr, &_vk_descriptor_set_layout) != VK_SUCCESS) {
    std::cerr << "Failed to create descriptor set layout" << std::endl;
    return false;
  }

  return true;
}

bool Renderer::InitDescriptorPool() {
  std::array<VkDescriptorPoolSize, 2> poolSizes = {};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(_vk_swapchain_images.size());
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = static_cast<uint32_t>(_vk_swapchain_images.size());

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = static_cast<uint32_t>(_vk_swapchain_images.size());

  if (vkCreateDescriptorPool(_vk_logical_device, &poolInfo, nullptr, &_vk_descriptor_pool) != VK_SUCCESS) {
    std::cerr << "Failed to create descriptor pool" << std::endl;
    return false;
  }
  
  return true;
}

bool Renderer::InitDescriptorSets() {
  std::vector<VkDescriptorSetLayout> layouts(_vk_swapchain_images.size(), _vk_descriptor_set_layout);
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = _vk_descriptor_pool;
  allocInfo.descriptorSetCount = static_cast<uint32_t>(_vk_swapchain_images.size());
  allocInfo.pSetLayouts = layouts.data();

  _vk_descriptor_sets.resize(_vk_swapchain_images.size());
  if (vkAllocateDescriptorSets(_vk_logical_device, &allocInfo, _vk_descriptor_sets.data()) != VK_SUCCESS) {
    std::cerr << "Failed to allocate descriptor sets" << std::endl;
    return false;
  }

  for (size_t i = 0; i < _vk_swapchain_images.size(); i++) {
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = _vk_uniform_buffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = _vk_texture_image_view;
    imageInfo.sampler = _vk_texture_sampler;

    std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = _vk_descriptor_sets[i];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = _vk_descriptor_sets[i];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(_vk_logical_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
  }

  return true;
}

bool Renderer::InitRenderPass() {
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = _vk_swapchain_image_format;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;

  if (vkCreateRenderPass(_vk_logical_device, &renderPassInfo, nullptr, &_vk_render_pass) != VK_SUCCESS) {
    std::cerr << "Failed to create render pass" << std::endl;
    return false;
  }

  return true;
}

bool Renderer::InitFramebuffers() {
  _vk_swapchain_framebuffers.resize(_vk_swapchain_image_views.size());

  for (size_t i = 0; i < _vk_swapchain_image_views.size(); i++) {
    VkImageView attachments[] = {
      _vk_swapchain_image_views[i]
      };

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = _vk_render_pass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = _vk_swapchain_extent.width;
    framebufferInfo.height = _vk_swapchain_extent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(_vk_logical_device, &framebufferInfo, nullptr, &_vk_swapchain_framebuffers[i]) != VK_SUCCESS) {
      std::cerr << "Failed to create framebuffer" << std::endl;
      return false;
    }
  }

  return true;
}

bool Renderer::InitCommandPool() {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = _device_manager.GetOperationQueueIndex(VK_QUEUE_GRAPHICS_BIT);

  if (vkCreateCommandPool(_vk_logical_device, &poolInfo, nullptr, &_vk_command_pool) != VK_SUCCESS) {
    std::cerr << "Failed to create vulkan command pool" << std::endl;
    return false;
  }

  return true;
}

bool Renderer::InitTextureImage() {
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load("Atlas.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    std::cerr << "Failed to load texture image" << std::endl;
    return false;
  }

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  InitBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(_vk_logical_device, stagingBufferMemory, 0, imageSize, 0, &data);
      memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(_vk_logical_device, stagingBufferMemory);

  stbi_image_free(pixels);

  if (!InitImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
    VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vk_texture_image, _vk_texture_image_memory))
    return false;

  if (!TransitionImageLayout(_vk_texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
    return false;
      
    CopyBufferToImage(stagingBuffer, _vk_texture_image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

  if (!TransitionImageLayout(_vk_texture_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    return false;

  vkDestroyBuffer(_vk_logical_device, stagingBuffer, nullptr);
  vkFreeMemory(_vk_logical_device, stagingBufferMemory, nullptr);

  return true;
}

bool Renderer::InitTextureImageView() {
  return InitImageView(_vk_texture_image_view, _vk_texture_image, VK_FORMAT_R8G8B8A8_UNORM);
}

bool Renderer::InitTextureSampler() {
  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = 16;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(_vk_logical_device, &samplerInfo, nullptr, &_vk_texture_sampler) != VK_SUCCESS) {
    std::cerr << "Failed to create texture sampler" << std::endl;
    return false;
  }

  return true;
}

bool Renderer::InitImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateImage(_vk_logical_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
    std::cerr << "Failed to create image" << std::endl;
    return false;
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(_vk_logical_device, image, &memRequirements);

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;

  if (int idx = _device_manager.GetCurrentDevice()->GetMemoryTypeIndex(memRequirements.memoryTypeBits, properties) >= 0) {
    allocInfo.memoryTypeIndex = idx;
  } else {
    std::cerr << "Failed to find suitable memory type" << std::endl;
    return false;
  }

  if (vkAllocateMemory(_vk_logical_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
    std::cerr << "Failed to allocate image memory" << std::endl;
    return false;
  }

  vkBindImageMemory(_vk_logical_device, image, imageMemory, 0);

  return true;
}

bool Renderer::InitImageView(VkImageView& imageView, VkImage image, VkFormat format) {
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(_vk_logical_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
    std::cerr << "Failed to create texture image view" << std::endl;
    return false;
  }

  return true;
}

bool Renderer::InitImageViews() {
  _vk_swapchain_image_views.resize(_vk_swapchain_images.size());

  for (size_t i = 0; i < _vk_swapchain_images.size(); i++) {
    if (!InitImageView(_vk_swapchain_image_views[i],_vk_swapchain_images[i], _vk_swapchain_image_format))
      return false;
  }

  return true;
}

bool Renderer::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    std::cerr << "Unsupported layout transition" << std::endl;
    return false;
  }

  vkCmdPipelineBarrier(
      commandBuffer,
      sourceStage, destinationStage,
      0,
      0, nullptr,
      0, nullptr,
      1, &barrier
  );

  EndSingleTimeCommands(commandBuffer);

  return true;
}

bool Renderer::InitCommandBuffers() {
  _vk_command_buffers.resize(_vk_swapchain_framebuffers.size());

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = _vk_command_pool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = _vk_command_buffers.size();

  if (vkAllocateCommandBuffers(_vk_logical_device, &allocInfo, _vk_command_buffers.data()) != VK_SUCCESS) {
    std::cerr << "Failed to allocate vulkan command buffers" << std::endl;
    return false;
  }

  for (size_t i = 0; i < _vk_command_buffers.size(); i++) {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(_vk_command_buffers[i], &beginInfo) != VK_SUCCESS) {
      std::cerr << "Failed to begin recording vulkan command buffer" << std::endl;
    }

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _vk_render_pass;
    renderPassInfo.framebuffer = _vk_swapchain_framebuffers[i];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = _vk_swapchain_extent;

    VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(_vk_command_buffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(_vk_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _vk_graphics_pipeline);

          VkBuffer vertexBuffers[] = {_vk_vertex_buffer};
          VkDeviceSize offsets[] = {0};
          vkCmdBindVertexBuffers(_vk_command_buffers[i], 0, 1, vertexBuffers, offsets);

          vkCmdBindIndexBuffer(_vk_command_buffers[i], _vk_index_buffer, 0, VK_INDEX_TYPE_UINT16);

          vkCmdBindDescriptorSets(_vk_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _vk_pipeline_layout, 0, 1, &_vk_descriptor_sets[i], 0, nullptr);

          vkCmdDrawIndexed(_vk_command_buffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(_vk_command_buffers[i]);

    if (vkEndCommandBuffer(_vk_command_buffers[i]) != VK_SUCCESS) {
      std::cerr << "Failed to record vulkan command buffer" << std::endl;
      return false;
    }
  }

  return true;
}

bool Renderer::InitSyncObjects() {
  _vk_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
  _vk_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
  _vk_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(_vk_logical_device, &semaphoreInfo, nullptr, &_vk_image_available_semaphores[i]) != VK_SUCCESS ||
      vkCreateSemaphore(_vk_logical_device, &semaphoreInfo, nullptr, &_vk_render_finished_semaphores[i]) != VK_SUCCESS ||
      vkCreateFence(_vk_logical_device, &fenceInfo, nullptr, &_vk_in_flight_fences[i]) != VK_SUCCESS) {
      std::cerr << "Failed to create synchronization objects for a frame" << std::endl;
      return false;
    }
  }

  return true;
}

bool Renderer::InitVertexBuffer() {
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  if (!InitBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory)) {
    return false;
  }

  void* data;
  vkMapMemory(_vk_logical_device, stagingBufferMemory, 0, bufferSize, 0, &data);
      memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
  vkUnmapMemory(_vk_logical_device, stagingBufferMemory);

  if (!InitBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vk_vertex_buffer, _vk_vertex_buffer_memory)) {
    return false;
  }

  CopyBuffer(stagingBuffer, _vk_vertex_buffer, bufferSize);

  vkDestroyBuffer(_vk_logical_device, stagingBuffer, nullptr);
  vkFreeMemory(_vk_logical_device, stagingBufferMemory, nullptr);

  return true;
}

bool Renderer::InitBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, 
  VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
  VkBufferCreateInfo bufferInfo = {};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(_vk_logical_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
    std::cerr << "Failed to create buffer" << std::endl;
    return false;
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(_vk_logical_device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  if (int idx = _device_manager.GetCurrentDevice()->GetMemoryTypeIndex(memRequirements.memoryTypeBits, properties) >= 0) {
    allocInfo.memoryTypeIndex = idx;
  } else {
    std::cerr << "Failed to find suitable memory type" << std::endl;
    return false;
  }

  if (vkAllocateMemory(_vk_logical_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
    std::cerr << "Failed to allocate buffer memory" << std::endl;
    return false;
  }


  vkBindBufferMemory(_vk_logical_device, buffer, bufferMemory, 0);

 return true;
}

void Renderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkBufferCopy copyRegion = {};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  EndSingleTimeCommands(commandBuffer);
}

void Renderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
  VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
      width,
      height,
      1
  };

  vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  EndSingleTimeCommands(commandBuffer);
}

bool Renderer::InitIndexBuffer() {
  VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  InitBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(_vk_logical_device, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, indices.data(), (size_t) bufferSize);
  vkUnmapMemory(_vk_logical_device, stagingBufferMemory);

  InitBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _vk_index_buffer, _vk_index_buffer_memory);

  CopyBuffer(stagingBuffer, _vk_index_buffer, bufferSize);

  vkDestroyBuffer(_vk_logical_device, stagingBuffer, nullptr);
  vkFreeMemory(_vk_logical_device, stagingBufferMemory, nullptr);

  return true;
}

const std::vector<const char*> Renderer::GetRequiredExtensions() const {
  return _vk_required_extenstions;
}

bool Renderer::InitUniformBuffers() {
  VkDeviceSize bufferSize = sizeof(UniformBufferObject);

  _vk_uniform_buffers.resize(_vk_swapchain_images.size());
  _vk_uniform_buffers_memory.resize(_vk_swapchain_images.size());

  for (size_t i = 0; i < _vk_swapchain_images.size(); i++) {
    if (!InitBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _vk_uniform_buffers[i], _vk_uniform_buffers_memory[i]))
      return false;
  }

  return true;
}

void Renderer::UpdateUniformBuffer(uint32_t currentImage) {
  UniformBufferObject ubo = {};
  float camera_x = 0, camera_y = 0;
  ubo.view = glm::translate(glm::mat4(1.0), glm::vec3(camera_x, camera_y, 0.0f));
  ubo.proj = glm::ortho(0.0f, 1280.f, 0.0f, 720.f, -1.0f, 1.0f);

  void* data;
  vkMapMemory(_vk_logical_device, _vk_uniform_buffers_memory[currentImage], 0, sizeof(ubo), 0, &data);
      memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(_vk_logical_device, _vk_uniform_buffers_memory[currentImage]);
}

void Renderer::DrawFrame() {
  vkWaitForFences(_vk_logical_device, 1, &_vk_in_flight_fences[_current_frame], VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(_vk_logical_device, _vk_swapchain, UINT64_MAX, 
    _vk_image_available_semaphores[_current_frame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    ResetSwapChain();
    std::cout << _vk_swapchain_extent.width << std::endl;
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    std::cerr << "Failed to acquire swap chain image" << std::endl;
  }

  UpdateUniformBuffer(imageIndex);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {_vk_image_available_semaphores[_current_frame]};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &_vk_command_buffers[imageIndex];

  VkSemaphore signalSemaphores[] = {_vk_render_finished_semaphores[_current_frame]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  vkResetFences(_vk_logical_device, 1, &_vk_in_flight_fences[_current_frame]);

  if (vkQueueSubmit(_vk_graphics_queue, 1, &submitInfo, _vk_in_flight_fences[_current_frame]) != VK_SUCCESS) {
    std::cerr << "Failed to submit draw command buffer" << std::endl;
  }

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {_vk_swapchain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;

  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(_vk_present_queue, &presentInfo);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
    framebuffer_resized = false;
    ResetSwapChain();
  } else if (result != VK_SUCCESS) {
    std::cerr << "Failed to present swap chain image" << std::endl;
  }

  _current_frame = (_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

VkCommandBuffer Renderer::BeginSingleTimeCommands() {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = _vk_command_pool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(_vk_logical_device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void Renderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(_vk_graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(_vk_graphics_queue);

  vkFreeCommandBuffers(_vk_logical_device, _vk_command_pool, 1, &commandBuffer);
}
