#include <iostream>
#include <fstream>
#include <experimental/filesystem>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"


#define SYSTEM_ERROR "system error: "


void exit_func(std::string msg){
    std::cerr << SYSTEM_ERROR << msg << std::endl;

    exit(EXIT_FAILURE);
}


void start_communication(const server_setup_information& setup_info, live_server_info& server)
{
    // HUJI - Create socket, bind and listen
    
    // Create a socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        #if DEBUG
        std::cerr << "Failed to create socket" << std::endl;
        #endif
        exit_func("Failed to create socket");
    }

    // Bind the socket to a specific port
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all available network interfaces
    serverAddress.sin_port = htons(setup_info.port);

    if (bind(serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        #if DEBUG
        std::cerr << "Failed to bind socket" << std::endl;
        #endif
        close(serverSocket);
        exit_func("Failed to bind socket");
    }

    // Listen for incoming connections
    int backlog = 5; // Maximum number of pending connections in the backlog queue
    if (listen(serverSocket, backlog) == -1) {
        #if DEBUG
        std::cerr << "Failed to listen for connections" << std::endl;
        #endif
        close(serverSocket);
        exit_func("Failed to listen for connections");
    }

    #if DEBUG
    std::cout << "Server listening on port " << setup_info.port << std::endl;
    #endif
    server.server_fd = serverSocket;

    // Generate a unique key based on the pathname and project ID
    key_t shm_key = ftok(setup_info.shm_pathname.c_str(), setup_info.shm_proj_id);
    if (shm_key == -1) {
        #if DEBUG
        std::cerr << "Failed to generate shared memory key" << std::endl;
        #endif
        close(serverSocket);
        exit_func("Failed to generate shared memory key");
    }

    // Create or open the shared memory segment
    int shm_id = shmget(shm_key, SHARED_MEMORY_SIZE, IPC_CREAT | 0666);
    if (shm_id == -1) {
        #if DEBUG
        std::cerr << "Failed to create/open shared memory segment" << std::endl;
        #endif
        close(serverSocket);
        exit_func("Failed to create/open shared memory segment");
    }

    #if DEBUG
    std::cout << "Shared Memory ID: " << shm_id << std::endl;
    #endif
    server.shmid = shm_id;
}


void write_to_info_file(const server_setup_information &setup_info,
                        const char *ipStr, std::ofstream &file) {
    file << ipStr << "\n";  // Write IP address followed by a new line

    file << setup_info.port << "\n";  // Write port number followed by a new line

    file << setup_info.shm_pathname << "\n";  // Write shared memory pathname followed by a new line

    file << setup_info.shm_proj_id << "\n";  // Write shared memory project ID followed by a new line
}


void create_info_file(const server_setup_information& setup_info, live_server_info& server)
{
    // Get socket bind address
    sockaddr_in address{};
    socklen_t addressLength = sizeof(address);

    if (getsockname(server.server_fd, reinterpret_cast<struct sockaddr*>(&address), &addressLength) == -1) {
        #if DEBUG
        std::cerr << "Failed to get socket address" << std::endl;
        #endif
        close(server.server_fd);
        shmctl(server.shmid, IPC_RMID, nullptr);
        exit_func("Failed to get socket address");
    }
    char ipStr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(address.sin_addr), ipStr, INET_ADDRSTRLEN) == nullptr) {
        #if DEBUG
        std::cerr << "Failed to convert IP address to string" << std::endl;
        #endif
        close(server.server_fd);
        shmctl(server.shmid, IPC_RMID, nullptr);
        exit_func("Failed to convert IP address to string");
    }
    #if DEBUG
    std::cout << "IPv4 Address: " << ipStr << std::endl;
    std::cout << "Port: " << ntohs(address.sin_port) << std::endl;
    #endif

    // Open file handle
    std::string filePath = setup_info.info_file_directory + "/" + setup_info.info_file_name;
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        #if DEBUG
        std::cerr << "Failed to open the file for writing" << std::endl;
        #endif
        close(server.server_fd);
        shmctl(server.shmid, IPC_RMID, nullptr);
        exit_func("Failed to convert IP address to string");
    }

    // write the data into the file
    write_to_info_file(setup_info, ipStr, file);

    file.close();
    server.info_file_path = filePath;
}


void get_connection(live_server_info& server)
{
    // Set up file descriptors for select
    fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(server.server_fd, &readFds);

    timeval timeout{};
    timeout.tv_sec = 120; // Timeout in seconds
    timeout.tv_usec = 0;

    #if DEBUG
    std::cout <<"server.server_fd: " << server.server_fd << std::endl;
    #endif

    // Wait for incoming connections or timeout
    int activity = select(server.server_fd + 1, &readFds, nullptr, nullptr, &timeout);
    if (activity == -1) {
        #if DEBUG
        std::cerr << "Failed to perform select" << std::endl;
        #endif
        server.server_fd = -1; // No connection established, store -1 in server_fd
        return;
    }
    else if (activity == 0) {
        #if DEBUG
        std::cout << "Timeout occurred. No incoming connections." << std::endl;
        #endif
        server.client_fd = -1; // No connection established, store -1 in client_fd
        return;
    }
    #if DEBUG
    std::cout <<"select_result: " << activity << std::endl;
    #endif

    sockaddr_in clientAddress{};
    socklen_t clientAddressLength = sizeof(clientAddress);
    int clientSocket = accept(server.server_fd, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddressLength);
    if (clientSocket == -1) {
        #if DEBUG
        std::cerr << "Failed to accept connection" << std::endl;
        #endif
        server.client_fd = -1; // No connection established, store -1 in client_fd
        return;
    }
    server.client_fd = clientSocket;

}

void write_to_socket(const live_server_info& server, const std::string& msg)
{
    const char * msg_ptr = msg.c_str();
    ssize_t bytesSent = send(server.server_fd + 1, msg.c_str(), msg.length(), 0);
    if (bytesSent == -1) {
        #if DEBUG
        std::cerr << "Failed to write message to socket" << std::endl;
        #endif
        return;
    }

    if (static_cast<size_t>(bytesSent) != msg.length()) {
        #if DEBUG
        std::cerr << "Incomplete write to socket" << std::endl;
        #endif
        return;
    }
}

void write_to_shm(const live_server_info& server, const std::string& msg)
{
    // Attach the shared memory segment to the process's address space
    void* shm_ptr = shmat(server.shmid, nullptr, 0);
    if (shm_ptr == reinterpret_cast<void*>(-1)) {
        #if DEBUG
        std::cerr << "Failed to attach shared memory segment" << std::endl;
        #endif
        close(server.server_fd);
        shmctl(server.shmid, IPC_RMID, nullptr);
        std::remove(server.info_file_path.c_str());
        exit_func("Failed to attach shared memory segment");
    }
    snprintf(static_cast<char*>(shm_ptr), SHARED_MEMORY_SIZE, "%s", msg.c_str()); // SHARED_MEMORY_SIZE
    
    // Detach the shared memory segment
    if (shmdt(shm_ptr) == -1) {
        #if DEBUG
        std::cerr << "Failed to detach shared memory segment" << std::endl;
        #endif
        close(server.server_fd);
        shmctl(server.shmid, IPC_RMID, nullptr);
        std::remove(server.info_file_path.c_str());
        exit_func("Failed to detach shared memory segment");
    }
}
void shutdown(const server_setup_information& setup_info, const live_server_info& server)
{
    // Delete the shared memory segment
    if (shmctl(server.shmid, IPC_RMID, nullptr) == -1) {
        #if DEBUG
        std::cerr << "Failed to delete shared memory segment" << std::endl;
        #endif
        exit_func("Failed to delete shared memory segment");
    }

    // Close the server socket
//    if (close(server.server_fd) == -1) {
    if (close(server.server_fd) < 0) {
        #if DEBUG
        std::cerr << "Failed to close server socket" << std::endl;
        #endif
        exit_func("Failed to close server socket");
    }

    if (std::remove(server.info_file_path.c_str()) != 0) {
        std::string message = "Failed to delete the file: ";
        #if DEBUG
        std::cerr << "Failed to delete the file: " << server.info_file_path.c_str() << std::endl;
        #endif

        exit_func(message +  server.info_file_path);
    }
    #if DEBUG
    std::cout << "was shutdown correctly";
     #endif

}
void run(const server_setup_information& setup_info, const std::string& shm_msg, const std::string& socket_msg)
{
    live_server_info server{};
    start_communication(setup_info, server);
    create_info_file(setup_info, server);
    write_to_shm(server, shm_msg);
    get_connection(server);
    if(server.client_fd != -1){
        write_to_socket(server, socket_msg);
    }
    sleep(10);
    shutdown(setup_info, server);
}