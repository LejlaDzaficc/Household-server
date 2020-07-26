#ifndef USER
#define USER

class User{

  int fd;

  public:

  int getFd()const{ return fd;};
  void setFd(int Fd){ fd = Fd;};

};

#endif
