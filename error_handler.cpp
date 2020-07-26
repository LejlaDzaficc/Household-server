#include <iostream>
#include <string>

void argument_error_handler(std::string message)
{
  printf("%s", message.c_str());
  exit(0);

}

void error_handler(std::string message)
{
  printf("%s\n", message.c_str());
  perror("Error: ");
  exit(0);
}
