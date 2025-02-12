#ifndef IRC_HPP
# define IRC_HPP

# include <cstring>
# include <unistd.h>
# include <cstdlib>
# include <iostream>
# include <string>
# include "Client.hpp"
# include "Server.hpp"

# include "Server.hpp"

/*-----------COLORS------------*/
# define GRE "\e[1;32m"
# define RED "\e[1;31m"
# define YEL "\e[1;33m"
# define RES "\033[0m"
# define WHI "\e[1;37m"

class Server;
class Client;

int	 parseInput(std::string password, int port);
std::string getCurrentTime();
std::string formatIRCMessage(const std::string& message);
int isAuthenticated(Client* client, Server* server, int fd);
bool checkNickInUse(std::vector<Client> clients, std::string &s);
bool validateNick(std::string &s);
void deleteRN(std::string &msg);
int	 parseInput(std::string password, int port);
std::vector<std::string> splitByDoublePoint(const std::string & msg);
std::string trimLeft(std::string &str);
std::string uppercase(std::string &s);

// void handleSIGINT(int signal);
// void handleSIGQUIT(int signal);

bool extractParams(Server* server, std::string command, std::string &msg, int fd); // by apardo-m
std::vector<std::string> split_msgAPM(std::string &msg); //by apardo-m
void deleteRN(std::string &msg);
int	 parseInput(std::string password, int port);
std::vector<std::string> splitByDoublePoint(const std::string & msg);
std::string trimLeft(std::string &str);
std::string uppercase(std::string &s);
std::string sanitizeInput(const std::string& msg);

#endif
