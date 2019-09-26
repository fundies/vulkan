#include "Client.hpp"

#include "flatbuffers/User_generated.h"

#include <iostream>

Client::~Client() {
  enet_host_destroy(_client);
}

bool Client::Connect(const char* host, unsigned port) {
  enet_address_set_host(&_address, host);
  _address.port = port;

  _client = enet_host_create (NULL /* create a client host */,
              1 /* only allow 1 outgoing connection */,
              2 /* allow up 2 channels to be used, 0 and 1 */,
              0 /* assume any amount of incoming bandwidth */,
              0 /* assume any amount of outgoing bandwidth */);

  if (_client == nullptr) {
    std::cerr << "An error occurred while trying to create an ENet client host" << std::endl;
    return false; 
  }

  /* Initiate the connection, allocating the two channels 0 and 1. */
  _peer = enet_host_connect (_client, &_address, 2, 0);    
  if (_peer == nullptr) {
    fprintf(stderr, "No available peers for initiating an ENet connection.\n");
    return false;
  }

  ENetEvent event;
  /* Wait up to 5 seconds for the connection attempt to succeed. */
  if (enet_host_service (_client, & event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
      std::cerr << "Connection to " << host << ":" << port << " succeeded" << std::endl;
  } else {
      std::cerr << "Connection to " << host << ":" << port << " failed" << std::endl;
  }
  
  return true;
}

void Client::Poll() {
  ENetEvent event;
  /* Wait up to 1000 milliseconds for an event. */
  while (enet_host_service (_client, & event, 1000) > 0) {
    switch(event.type) {
      case ENET_EVENT_TYPE_RECEIVE: {
        flatbuffers::FlatBufferBuilder builder;
        auto user = GetUser(event.packet->data);
        std::cout << user->name()->str() << "@" << user->ip()->str() << std::endl;
      }
    }
  }
}
