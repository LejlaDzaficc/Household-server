#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
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

std::set<int> fdContainer;
std::list<Household> households;
std::mutex mtxFdContainer;
std::mutex mtxHouseholds;

const int numberOfPendingConections = 10;
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
 
  std::unique_lock<std::mutex> lckFdContainer(mtxFdContainer);
  fdContainer.erase(fd);
  std::string numberOfConnectedClients = "Number of connected clients: " + std::to_string(fdContainer.size()) + '\n';
  lckFdContainer.unlock();
  write(1, numberOfConnectedClients.c_str(), strlen(numberOfConnectedClients.c_str()));

  std::lock_guard<std::mutex> lckHouseholds{mtxHouseholds};
  for(auto it = households.begin(); it!= households.end(); ++it)
    (*it).removeUser(fd);
}

void handleChatMessage(json requestJson, int fd){
  std::string groupID = requestJson["groupID"];
  std::string message = requestJson.dump();
  std::lock_guard<std::mutex> lck{mtxHouseholds};
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
}

bool addUserToSpecificHousehold(std::string groupID, int fd){
  std::lock_guard<std::mutex> lck{mtxHouseholds};
  for(auto it = households.begin(); it!= households.end(); ++it){
    if((*it).getID() == groupID){
      User user = User();
      user.setFd(fd);
      (*it).addUser(user);
      return true;
    }
  }
  return false;
}

void handleJoinToHouseholdMessage(json requestJson, int fd){
  std::string groupID = requestJson["groupID"];
  json responseJson;

  bool isUserAdded = addUserToSpecificHousehold(groupID, fd);
  if(!isUserAdded){
    responseJson["command"] = "joinToHousehold";
    responseJson["status"] = "rejected";
    responseJson["reason"] = "Household with given id does not exist!";
  }
  else{
    responseJson["command"] = "joinToHousehold";
    responseJson["status"] = "accepted";   
  }
  std::string message = responseJson.dump();
  write(fd, message.c_str(), strlen(message.c_str()));
}

std::string generateGroupID(){
  auto gen = boost::uuids::random_generator();
  boost::uuids::uuid ID = gen();
  return boost::uuids::to_string(ID);
}

void handleCreateNewHouseholdMessage(int fd){
  std::string groupID = generateGroupID();
  Household household = Household(groupID);
  std::unique_lock<std::mutex> lck(mtxHouseholds);
  households.push_back(household);
  lck.unlock();
  json responseJson;

  bool isUserAdded = addUserToSpecificHousehold(groupID, fd);
  if(!isUserAdded){
    responseJson["command"] = "createNewHousehold";
    responseJson["status"] = "rejected";
    responseJson["reason"] = "Household could not been created!\nPlease try again later.";
  }
  else{
    responseJson["command"] = "createNewHousehold";
    responseJson["status"] = "accepted";  
    responseJson["groupID"] = groupID;
  }
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
    if(command == "createNewHousehold")
      handleCreateNewHouseholdMessage(fd);
    else if(command == "joinToHousehold")
      handleJoinToHouseholdMessage(requestJson, fd);
    else if(command == "chatMessage")
      handleChatMessage(requestJson, fd);
  }
}

void serveClient(int newClientFd){
  while(1){
    std::unique_lock<std::mutex> lck(mtxFdContainer);
    bool isClientConnected = fdContainer.find(newClientFd) != fdContainer.end();
    lck.unlock();
    if(!isClientConnected)
      break;
    handleNewMessage(newClientFd);
  }
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
    std::unique_lock<std::mutex> lck(mtxFdContainer);
    fdContainer.insert(newClientFd);
    std::string numberOfConnectedClients = "Number of connected clients: " + std::to_string(fdContainer.size()) + '\n';
    lck.unlock();
   
    std::thread client{serveClient, newClientFd};
    client.detach();
    write(1, numberOfConnectedClients.c_str(), strlen(numberOfConnectedClients.c_str()));
  }
}

void runServer(int port){
  int serverFd = prepareServer(port);
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
