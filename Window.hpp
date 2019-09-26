#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "Renderer.hpp"

#include <vector>

class Window {
public:
  Window() = default;
  ~Window();
  bool Init(unsigned window_width, unsigned window_height, const char* window_caption);
  void Poll();

  bool game_ending = false;

protected:
  std::vector<const char*> GetExtensions();

  GLFWwindow* _window;
  Renderer _renderer;
};

#endif
