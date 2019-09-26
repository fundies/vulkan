#include "Game.hpp"


int main() {
  /*Client client;
  if (!client.Connect("127.0.0.1", 1234))
    return EXIT_FAILURE;
 
  //std::cout << "User: " << user.name << std::endl;

  while(true) client.Poll();*/

  Game game;
  if (!game.Init(1280, 720, "Game"))
    return EXIT_FAILURE;

  while(!game.GameEnding()) game.Update();
  
  return EXIT_SUCCESS;
}
