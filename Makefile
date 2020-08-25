all: server.cpp connectionTime.cpp error_handler.cpp Household.cpp
	clang++ -I /usr/local/boost1_68_0/include -std=c++14 -o server server.cpp connectionTime.cpp error_handler.cpp Household.cpp -lpthread
