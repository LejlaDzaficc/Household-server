#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <ctype.h>
#include <set>
#include <mutex>
#include <thread>
#include "Household.h"
#include "json.hpp"

#define MAXSIZE 512

extern const std::string getCurrentDateTime();
extern void argument_error_handler(std::string message);
extern void error_handler(std::string message);

std::mutex mtx;
const int numberOfPendingConections = 10;
std::set<int> fdContainer;
std::list<Household> households;
using json = nlohmann::json;

int prepareServer(int port){
  struct sockaddr_in serverAddr;
  char buffer[1024];
  int serverFd = socket(AF_INET, SOCK_STREAM, 0);
  if(serverFd < 0)
    error_handler("Error in socket()!");   
 
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(serverFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    error_handler("Error in bind()!");
  if(listen(serverFd, numberOfPendingConections) < 0)
    error_handler("Error in listen()!"); 
  return serverFd;
}

void closeConnection(int fd, fd_set *readFd_set){
  close(fd);
  if(readFd_set != nullptr)
    FD_CLR(fd, readFd_set);
  std::lock_guard<std::mutex> lck{mtx};
  fdContainer.erase(fd);
  for(auto it = households.begin(); it!= households.end(); ++it)
    (*it).removeUser(fd);
  std::string numberOfConnectedClients = "Number of connected clients: " + std::to_string(fdContainer.size()) + '\n';
  write(1, numberOfConnectedClients.c_str(), strlen(numberOfConnectedClients.c_str()));
}

void handleChatMessage(json requestJson, int fd){
  std::string groupID = requestJson["groupID"];
  std::string message = requestJson.dump();
  std::lock_guard<std::mutex> lck{mtx};
  /* Send/forward message to other participants in household */
  for(auto it = households.begin(); it!= households.end(); ++it){
    if((*it).getID() == groupID){
      std::list<User> users = (*it).getUsers();
      for(auto iter = users.begin(); iter!= users.end(); ++iter){
        if((*iter).getFd() != fd)
          write((*iter).getFd(), message.c_str(), strlen(message.c_str()));
      }
    }
  }
  mtx.unlock();
  write(1, message.c_str(), strlen(message.c_str()));
}

void handleJoinToHouseholdMessage(json requestJson, int fd){
  std::string groupID = requestJson["groupID"];
  std::lock_guard<std::mutex> lck{mtx};
  bool isGroupIDFound = false;
  json responseJson;
  for(auto it = households.begin(); it!= households.end(); ++it){
    if((*it).getID() == groupID){
      isGroupIDFound = true;
      User user = User();
      user.setFd(fd);
      (*it).addUser(user);
    }
  }
  if(!isGroupIDFound){
    responseJson["command"] = "joinToHousehold";
    responseJson["status"] = "rejected";
    responseJson["reason"] = "Household with given id does not exist!";
  }
  else{
    responseJson["command"] = "joinToHousehold";
    responseJson["status"] = "accepted";   
  }
  mtx.unlock();
  std::string message = responseJson.dump();
  write(fd, message.c_str(), strlen(message.c_str()));
}

void handleNewMessage(int fd, fd_set *readFd_set = nullptr){
  char buffer[MAXSIZE];
  int readBytes;

  readBytes = read(fd, buffer, MAXSIZE);
  if(readBytes < 0)
    error_handler("Error in read()!");
  else if(readBytes == 0)
    closeConnection(fd, readFd_set);
  else{
    buffer[readBytes] = '\0';
    auto requestJson = json::parse(std::string(buffer));
    std::string command = requestJson["command"];
    if(command == "joinToHousehold")
      handleJoinToHouseholdMessage(requestJson, fd);
    else if(command == "chatMessage")
      handleChatMessage(requestJson, fd);
  }
}

void serveClient(int newClientFd){
  while(fdContainer.find(newClientFd) != fdContainer.end())
    handleNewMessage(newClientFd);
}

void handleConnections(int serverFd){
  struct sockaddr_in clientAddr;
  socklen_t size = sizeof(clientAddr);
  int newClientFd;
  while(1){
    if((newClientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &size)) < 0)
      error_handler("Error in accept()!");
    std::string clientConnectedMessage = "NEW CLIENT CONNECTED!\n\tClient address: '" 
      + std::string(inet_ntoa(clientAddr.sin_addr)) + ":" 
      + std::to_string(ntohs(clientAddr.sin_port)) 
      + "'\n\tTime: " + getCurrentDateTime().c_str() + "\n";
    write(1, clientConnectedMessage.c_str(), strlen(clientConnectedMessage.c_str()));
    std::thread client{serveClient, newClientFd};
    client.detach();
    std::lock_guard<std::mutex> lck{mtx};
    fdContainer.insert(newClientFd);
    std::string numberOfConnectedClients = "Number of connected clients: " + std::to_string(fdContainer.size()) + '\n';
    mtx.unlock();
    write(1, numberOfConnectedClients.c_str(), strlen(numberOfConnectedClients.c_str()));
  }
}

void runServer(int port){
  int serverFd = prepareServer(port);
  /* Hardcoded to support only two households */
  Household household1 = Household("1");
  Household household2 = Household("2");
  households.push_back(household1);
  households.push_back(household2);

  handleConnections(serverFd);
}

int getPortNumberFromArguments(int argc, char* argv[]){
  int port;
  if(argc != 2)
    argument_error_handler("Error in parseArguments(): Invalid number of arguments!\n");
  else if((port = atoi(argv[1])) <= 0)
    argument_error_handler("Error in parseArguments(): Missing port number!\n");
  return port;
}

int main(int argc, char* argv[]){
  int port = getPortNumberFromArguments(argc, argv);
  runServer(port);
}
