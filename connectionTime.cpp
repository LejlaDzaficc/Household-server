#include <iostream>
#include <time.h>

const std::string getCurrentDateTime()
{
  time_t now = time(0);
  char buffer[80];
  struct tm timeStruct = *localtime(&now);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d: %X", &timeStruct);
  return buffer;
}
