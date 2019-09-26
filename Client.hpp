#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <enet/enet.h>

class Client {
public:
  Client() = default;
  ~Client();
  bool Connect(const char* host, unsigned port);
  void Poll();

protected:
  ENetAddress _address;
  ENetPeer* _peer;
  ENetHost* _client;
};

#endif
