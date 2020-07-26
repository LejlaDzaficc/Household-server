#ifndef HOUSEHOLD
#define HOUSEHOLD
#include <iostream>
#include <list>
#include "User.h"

class Household{

  std::string id;
  std::list<User> users;

  public:

  /* Hardcoded to support only one household */
  Household(std::string ID);

  std::string getID()const{ return id;}
  std::list<User> getUsers()const{ return users;}

  void addUser(User user);
  void removeUser(int fd);
};


#endif
