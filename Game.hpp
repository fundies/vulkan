#ifndef GAME_HPP
#define GAME_HPP

#include "Window.hpp"

class Game {
public:
  Game() = default;
  ~Game() = default;
  bool Init(unsigned window_width, unsigned window_height, const char* window_caption);
  void Update();
  bool GameEnding();

protected:
  Window _window;
};

#endif
