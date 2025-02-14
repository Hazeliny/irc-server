#include "Server.hpp"
#include <arpa/inet.h>//para inet_ntoa que convierte una direccion ip en una cadena
#include "Messageprocessing.hpp"
#include <cerrno>
#include <cstdio>
// #include "replies.hpp"

Server::Server( void ) :_serverName("ircserv"), _password("password"), _port(50000), _fdServer(-1)
{}

Server::Server(std::string serverName, std::string password, int port) :_serverName(serverName), _password(password), _port(port), _fdServer(-1)
{
	// std::cout << "Server() => Set initial values" << std::endl;
}

//Function that creates the socket(_fdServer) and configures it.
void Server::createSocket()
{
    struct	sockaddr_in socketAddress;//estructura que almacena la dirección del socket (IP y puerto) para una conexión IPv4.
    int                 enableReuseAddr;
    
    // Crear y configurar el socket
	std::memset(socketAddress.sin_zero, 0, sizeof(socketAddress.sin_zero));//Rellena con ceros el resto de la estructura sockaddr_in.
	socketAddress.sin_family = AF_INET;//Especifica que el socket usará el protocolo IPv4.
	socketAddress.sin_port = htons(_port);//Establece el puerto del socket, convirtiéndolo a formato de red (big-endian) usando htons.
	socketAddress.sin_addr.s_addr = INADDR_ANY;//Establece la dirección IP del socket, en este caso, INADDR_ANY, que indica que el socket escuchará en todas las interfaces de red.

    // Crear el df para el socket
	_fdServer = socket(AF_INET, SOCK_STREAM, 0);
	if (_fdServer == -1)
		throw(std::logic_error("Failed to create socket"));

    // Configurar la reutilización de direcciones
	enableReuseAddr = 1;
	if(setsockopt(_fdServer, SOL_SOCKET, SO_REUSEADDR, &enableReuseAddr, sizeof(enableReuseAddr)) == -1) // configuramos el socket con SO_REUSEADDR para poder reutilizar puertos o ips
		throw(std::runtime_error("Failed to set option (SO_REUSEADDR) on socket"));

    // Configurar el socket como no bloqueante
	if (fcntl(_fdServer, F_SETFL, O_NONBLOCK) == -1)
		throw(std::runtime_error("Failed to set option (O_NONBLOCK) on socket"));

    // Vincular el socket a la dirección y puerto especificados
    if (bind(_fdServer, reinterpret_cast<struct sockaddr*>(&socketAddress), sizeof(socketAddress)) == -1) {
        throw std::runtime_error("Failed to bind socket");
    }
    // std::cout << "Socket created successfully." << std::endl;
}

//Function that listens for incoming connections.
void Server::listenSocket()
{
    // Escuchar conexiones entrantes
    if (listen(_fdServer, SOMAXCONN) == -1) // SOMAXCONN: n° máx de conexiones pendientes en la cola de conexiones.
	{
        throw std::runtime_error("Failed to listen on socket");
    }
    // std::cout << "Server is now listening for incoming connections." << std::endl;
}

//Function that fills the pollfd structure and adds it to the monitoring vector.
void Server::fillPollfd()
{
    // Configurar pollfd y agregar al vector de monitoreo
    struct pollfd serPoll;
    
    serPoll.fd = _fdServer;//Establece el descriptor de archivo a monitorear.
    serPoll.events = POLLIN;//Establece los eventos a monitorear en el descriptor de archivo.
    _fdsClients.push_back(serPoll);//Agrega el pollfd al vector de monitoreo.
    std::cout << GRE << "Server successfully connected on port " << _port << "." << RES << std::endl;
	std::cout << "Waiting for incoming connections..." << std::endl;
}

// Function to accept a new client connection
void Server::acceptClient()
{
        struct sockaddr_in  clientAddress;
        socklen_t           clientAddressSize;
        int                 connectionSocket;
        struct pollfd       clientPoll;
        Client              newClient;

        clientAddressSize = sizeof(clientAddress);
        connectionSocket = accept(_fdServer, (struct sockaddr*)&clientAddress, &clientAddressSize);
        if (connectionSocket == -1) {
            throw std::runtime_error("Failed to accept new client");
        }

        // Configure the client socket as non-blocking
        if (fcntl(connectionSocket, F_SETFL, O_NONBLOCK) == -1) {
            throw std::runtime_error("Failed to set option (O_NONBLOCK) on client socket");
        }

        // Add the client to the list of monitored FDs
        clientPoll.fd = connectionSocket;//
        clientPoll.events = POLLIN;
        clientPoll.revents = 0;

        newClient.setFdClient(connectionSocket);
        newClient.setIpClient(inet_ntoa(clientAddress.sin_addr));
        _clients.push_back(newClient);
        _fdsClients.push_back(clientPoll);
        std::cout << "New client connected\n";
}

// Function to remove a client based on its file descriptor
void Server::clearClients(int clientSocket, std::string msg)
{
        // Find the client in the list of monitored FDs
        std::vector<struct pollfd>::iterator it = _fdsClients.begin();
        for (; it != _fdsClients.end(); ++it) {
            if (it->fd == clientSocket) {
                break;  // Found the client
            }
        }

        // If found, erase the client from the list
        if (it != _fdsClients.end()) {
            _fdsClients.erase(it);
        }
        
        // Close the socket of the client
        close(clientSocket);
        std::cout << "fd = " << clientSocket << msg << std::endl;
}

// Function to split a string into a vector of strings
std::vector<std::string> splitStr(const std::string& input, char separator)//no se utiliza aún(250207)
{
    std::istringstream          stream(input);
    std::string                 token;
    std::vector<std::string>    ret;

    while (std::getline(stream, token, separator))
        ret.push_back(token);
    return (ret);
}

// Receive data from the client
void Server::receiveData(int clientSocket)
{
    char buffer[BUFFER_SIZE + 1];
    int bytesRead;
    Messageprocessing messageProcessing;

    bytesRead = recv(clientSocket, buffer, BUFFER_SIZE, 0);
    if (bytesRead == -1) {
        throw std::runtime_error("Failed to receive data from client");
    }
    else if (bytesRead == 0) {
        clearClientFromClientsAndChanels(clientSocket, "Client disconnected1\n");//solve the issue when a client disconnects abruptly & connect again
        return;
    }

    buffer[bytesRead] = '\0';
    std::cout << "Received data: " << buffer << std::endl;//debug
    // std::cout << "\nBuffer size: " << strlen(buffer) << std::endl;
    std::string message(buffer);

    Client* client = getClient(clientSocket);//Get the client from the client list
    if (!client) return;

    // Append the received message to the client's buffer ----> _bufferInMessage
    client->appendToBuffer(message);

    // Process the message
    while (client->hasCompleteCommand()) {
        std::string command = client->extractCommand();
        messageProcessing.processMessage(this, command, clientSocket);
    }
}


//Function that loops to monitor events on the fd.
void Server::loop()
{
    while (!Server::_Signal)
    {
        int pollRet;  // Stores the return value of the poll() function
        int revents;  // Stores the events that occurred in the fd

        // Wait for events on the file descriptors
        pollRet = poll(_fdsClients.data(), _fdsClients.size(), 1000);// 1000 ms a timeout to avoid blocking indefinitely
        if (pollRet == -1) {
            if (errno == EINTR) {
                // poll() was interrupted by a signal
                continue;
            }
            throw std::runtime_error("The function poll() failed");
        }

        // Iterate over the file descriptors to determine the event
        for (size_t i = 0; i < _fdsClients.size(); i++)
        {
            revents = _fdsClients[i].revents;

            // If no events, skip to the next iteration
            if (revents == 0) 
                continue;

            // Handle errors or unexpected disconnections
            if ((revents & POLLERR) == POLLERR || (revents & POLLHUP) == POLLHUP) 
            {
                std::cout << "Socket error or client disconnection\n";
                clearClients(_fdsClients[i].fd, "Client disconnected2\n");
            }
            else if (revents & POLLIN) {  // There is data to read
                if (_fdsClients[i].fd == _fdServer) {  // New incoming connection
                    acceptClient();
                }
                else {  // Message from an existing client
                    receiveData(_fdsClients[i].fd);
                }
                // Mark all fds as ready to write
                for (size_t j = 1; j < _fdsClients.size(); j++) {
                    _fdsClients[j].events |= POLLOUT;
                }
            }
        }
    }
}
 
void Server::   runServer()
{
	createSocket();
	listenSocket();
    std::cout << "IRC server is running. Press Ctrl+C to stop." << std::endl;
	fillPollfd();
	loop();

}

//Function that sends a response to the client.
void Server::sendResp(std::string msg, int clientFd) {
    ssize_t bytesSent = send(clientFd, msg.c_str(), msg.length(), 0);
    if (bytesSent == -1) {
        perror("sendResp failed"); // Muestra el error del sistema
    }
}

//Function that sends a response to all clients except the one that sent the message.
void Server::sendBroadAll(std::string resp)
{
	std::cout << "sendBroadAll() :" << std::endl;

	for (size_t i = 0; i < this->_clients.size(); i++)
		sendResp(resp, _clients[i].getFdClient());
}

//Function that sends a response to all clients except the one that sent the message.
void Server::sendBroad(std::string resp, int fd)
{
	int	actualFd;

	std::cout << "sendBroad() :" << std::endl;

	for (size_t i = 0; i < this->_clients.size(); i++)
	{
		actualFd = _clients[i].getFdClient();
		std::cout << " i,fd = " << actualFd << ", " << fd << std::endl;
		if (actualFd != fd)
		{
			std::cout << "  send  i,fd = " << actualFd << ", " << fd << std::endl;
			sendResp(resp, actualFd);
		}
	}
}

//Function that returns the client based on the file descriptor.
//Client *Server::getClient(std::vector<Client> clients, int fd)
Client *Server::getClient(int fd)
{
//	for (size_t i = 0; i < clients.size(); i++)
	std::vector<Client>& clientsRef = getClients();
	for (size_t i = 0; i < clientsRef.size(); i++)
	{
//		if (clients[i].getFdClient() == fd)
		if (clientsRef[i].getFdClient() == fd)
			return (&clientsRef[i]);
//			return (&(clients[i]));
	}
	return (NULL);
}

Client *Server::getClientByNick(std::string &nick)
{
    std::vector<Client>& clientsRef = getClients();
    for (size_t i = 0; i < clientsRef.size(); i++)
	{
        if (clientsRef[i].getNick() == nick)
            return (&clientsRef[i]);
    }
    return (NULL);
}

//-----------------------------Getters & Setters-----------------------------//
std::string	Server::getServerName( void ) const { return (this->_serverName); }
std::string	Server::getPassword( void ) const { return (this->_password); }
int 		Server::getPort( void ) const { return (this->_port); };
int			Server::getFdServer( void ) const { return (this->_fdServer); };
//std::vector<Channel> Server::getChannels( void ) { return (this->_channels); }
std::vector<Channel>& Server::getChannels( void ) { return (this->_channels); }
//std::vector<Client> Server::getClients( void ) { return (this->_clients); }
std::vector<Client>& Server::getClients( void ) { return (this->_clients); }

size_t	Server::getChannelsSize( void ) { return (this->_channels.size()); }

Channel *Server::getChannelByChanName(std::string channelName)
{
    for (size_t i = 0; i < this->getChannels().size(); i++)
    {
        if (this->getChannels()[i].getChannelName() == channelName)
            return (&(this->getChannels()[i]));
    }
    return (NULL);
}

void 		Server::addClient( Client newClient ) { this->_clients.push_back(newClient); }
void 		Server::addChannel( Channel newChannel ){ this->_channels.push_back(newChannel); }

void		Server::deleteClient( int fd )
{
	std::vector<Client>::iterator it = this->_clients.begin();
	while (it != _clients.end() && it->getFdClient() != fd)
		it++;
	// If found, erase the client from the list
	if (it != _clients.end())
            _clients.erase(it);
}

void		Server::deleteChannel( std::string chName )
{
	std::vector<Channel>::iterator it = this->_channels.begin();
	while (it != _channels.end() && it->getChannelName() != chName)
		it++;
	// If found, erase the client from the list
	if (it != _channels.end())
            _channels.erase(it);
}

// apardo-m need for QUIT
void		Server::clearClientFromClientsAndChanels( int fd, std::string msg)
{
	deleteClient( fd ); //delete client from _clients
	clearClients(fd, msg); //delete fd from _fdsClients
}

// apardo-m need for Topic
//Function that checks if a channel is in the list of channels.
bool		Server::isInChannels( std::string chName )
{
	std::vector<Channel>::iterator it = this->_channels.begin();
	while (it != _channels.end() && it->getChannelName() != chName)
		it++;
	if (it != _channels.end())    // found Channel
		return (true);
	return ( false );
}

Channel*   	Server::getChannelByChannelName( std::string chName )
{
	std::vector<Channel>::iterator it = this->_channels.begin();
	while (it != _channels.end() && it->getChannelName() != chName)
		it++;
	return (&(*it));
}

//For test proposal

Client*		Server::getClientByFD(int fd)
{
	 for (unsigned long i = 0 ; i < this->_clients.size(); i++)
	 {
		if (this->_clients[i].getFdClient() == fd)
			return (&(this->_clients[i]));
	 }
	 return (NULL);
}

Channel*	Server::getChannelsByNumPosInVector(size_t pos)
{
	 return (&(this->_channels[pos]));
}

Server::~Server()
{
    std::cout << "Closing connections..." << std::endl;
    // std::string msg = "Server is closing...!\n";

    for (size_t i = 0; i < _clients.size(); ++i) {
        int fd = _clients[i].getFdClient();
        if (fd != -1) {
            if (fcntl(fd, F_GETFD) != -1) { // si el socket aún es válido
                sendResp(ERR_SERVERDOWN(_clients[i].getNick()), fd);
            }
            close(fd);
        }
    }

    if (_fdServer != -1) {
        close(_fdServer);
    }

    _fdsClients.clear();
    _clients.clear();
    _channels.clear();
}
