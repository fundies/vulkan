#include "Game.hpp"

bool Game::Init(unsigned window_width, unsigned window_height, const char* window_caption) {
  if (!_window.Init(window_width, window_height, window_caption))
    return false;
  
  return true;
}

void Game::Update() {
  _window.Poll();
}

bool Game::GameEnding() {
  return _window.game_ending;
}
