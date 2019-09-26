#include "Window.hpp"

#include <iostream>
#include <chrono>

Window::~Window() {
  glfwDestroyWindow(_window);
  glfwTerminate();
}

bool Window::Init(unsigned window_width, unsigned window_height, const char* window_caption) {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  _window = glfwCreateWindow(window_width, window_height, window_caption, nullptr, nullptr);
  
  if (_window == nullptr) {
    std::cerr << "Failed to initialize window" << std::endl;
    return false;
  }

  glfwSetWindowUserPointer(_window, this);

  if (!_renderer.Init(_window, window_caption, window_caption, GetExtensions())) {
    std::cerr << "Failed to initialize renderer" << std::endl;
    return false;
  }

  auto updateSwapChain = [](GLFWwindow* w, int, int) {
    static_cast<Window*>(glfwGetWindowUserPointer(w))->_renderer.framebuffer_resized = true;
  };
  
  glfwSetFramebufferSizeCallback(_window, updateSwapChain);
  
  return true;
}

static double previousTime = 0;
static int frameCount = 0;

void Window::Poll() {
  while (!glfwWindowShouldClose(_window) && !game_ending) {
    glfwPollEvents();
    _renderer.DrawFrame();

    // Measure speed
    double currentTime = glfwGetTime();
    frameCount++;
    // If a second has passed.
    if ( currentTime - previousTime >= 1.0 )
    {
        // Display the frame count here any way you want.
        std::cout << "fps: " << frameCount << std::endl;

        frameCount = 0;
        previousTime = currentTime;
    }
  } game_ending = true;
}

std::vector<const char*> Window::GetExtensions() {
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  auto exts = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
  if (_renderer.DebugEnabled()) exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  //for (const auto& ext : exts) std::cout << "\t" << ext << std::endl;
  return exts;
}
