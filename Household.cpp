#include "Household.h"

Household::Household(std::string ID){ id = ID;}

void Household::addUser(User user){
  users.push_back(user);
}

void Household::removeUser(int fd){
    for(auto iter = users.begin(); iter!= users.end();)
    {
      if((*iter).getFd() == fd)
        iter = users.erase(iter);
      else
        ++iter;
    }
}

