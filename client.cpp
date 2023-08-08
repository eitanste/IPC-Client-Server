#include "client.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <dirent.h>
#include <algorithm>

#define SYSTEM_ERROR "system error: "


void exit_func(std::string msg){
    std::cerr << SYSTEM_ERROR << msg << std::endl;

    exit(EXIT_FAILURE);
}


struct file_data {
    unsigned short port;
    std::string ip_addr;
    std::string shm_pathname;
    int shm_proj_id;
};

file_data readFromFile(const std::string &filename) {
    std::vector<std::string> lines;
    file_data server_info{};

    std::ifstream file(filename);
    if (!file) {
#if DEBUG
        std::cerr << "Failed to open file: " << filename << std::endl;
#endif
        return server_info;
    }

    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    file.close();
    if (lines.size() != 4) {
#if DEBUG
        std::cerr << "File lines count is not 4 " << filename << std::endl;
#endif
        return server_info;
    }
    server_info.ip_addr = lines[0];
    server_info.port = atoi(lines[1].c_str());
    server_info.shm_pathname = lines[2];
    server_info.shm_proj_id = atoi(lines[3].c_str());
    return server_info;
}


int connect_to_socket(std::string &ip_addr, int port, int sockfd) {
    // Set the socket to non-blocking mode
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Initialize server address
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_addr.c_str(), &(serverAddress.sin_addr)) <= 0) {
#if DEBUG
        std::cerr << "Invalid IP address: " << ip_addr << std::endl;
#endif
        close(sockfd);
        exit_func("Invalid IP address: ");
    }

    // Set up the timeout
    timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    fd_set writeFds;
    FD_ZERO(&writeFds);
    FD_SET(sockfd, &writeFds);

    // Wait for the connection to complete or timeout
    int selectRes = select(sockfd + 1, nullptr, &writeFds, nullptr, &timeout);
    if (selectRes == -1) {
#if DEBUG
        std::cerr << "Error occurred during select syscall" << std::endl;
#endif
        close(sockfd);
        exit_func("Error occurred during select syscall");
    } else if (selectRes == 0) {
#if DEBUG
        std::cerr << "Connection timeout" << std::endl;
#endif
        close(sockfd);
        exit_func("Connection timeout");
    }

    // Set the socket back to blocking mode
    fcntl(sockfd, F_SETFL, flags);

    // Start connecting to the server
    int connectRes = connect(sockfd,
                             reinterpret_cast<sockaddr *>(&serverAddress),
                             sizeof(serverAddress));
    if (connectRes == -1) {
#if DEBUG
        std::cerr << "Failed to connect to the server" << std::endl;
#endif
        close(sockfd);
        return -1;
    }

    return sockfd;
}


int connect_to_shm(std::string &shm_path, int project_id) {
    key_t key = ftok(shm_path.c_str(), project_id);
    if (key == -1) {
#if DEBUG
        std::cerr << "Failed to generate key" << std::endl;
#endif
        return -1;
    }

    // Connect to the shared memory segment using the generated key
    int shmid = shmget(key, 0, 0);
    if (shmid == -1) {
#if DEBUG
        std::cerr << "Failed to connect to shared memory segment" << std::endl;
#endif
        return -1;
    }
    return shmid;
}


bool
compare(const live_server_info &server1, const live_server_info &server2) {
    return server1.info_file_path < server2.info_file_path;
}


int count_servers(const std::string &client_files_directory,
                  std::vector<live_server_info> &servers) {
    int servers_count = 0;
    // Check if directory exists
    DIR *dir = opendir(client_files_directory.c_str());
    if (!dir) {
#if DEBUG
        std::cerr << "Invalid directory path: " << client_files_directory
                  << std::endl;
#endif
        exit_func("Invalid directory path: ");
    }

    dirent *entry;

    while ((entry = readdir(dir)) != nullptr) {
        std::string file_name = entry->d_name;

        // Skip the current directory and parent directory entries
        if (file_name == "." || file_name == "..") {
            continue;
        }

        std::string file_path = client_files_directory;
        file_path.append("/").append(file_name);

        // Check if it's a regular file
        std::ifstream file(file_path);
        if (file && entry->d_type == DT_REG) {
            servers_count++;
            file_data server_info = readFromFile(file_path);
            int shmid = connect_to_shm(server_info.shm_pathname,
                                       server_info.shm_proj_id);
            // Create a socket
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1) {
#if DEBUG
                std::cerr << "Failed to create socket" << std::endl;
#endif
                exit_func("Failed to create socket");
            }
            int connection_suc = connect_to_socket(server_info.ip_addr,
                                             server_info.port, sockfd);

            live_server_info server_status{};
            server_status.info_file_path = file_path;
            server_status.server_fd = -1;

            if (connection_suc == -1) {
#if DEBUG
                std::cerr << "Could not connect to socket "
                          << server_info.ip_addr << ": " << server_info.port
                          << std::endl;
#endif
                server_status.client_fd = -1;
            } else {
                server_status.client_fd = sockfd;
            }
            if (shmid == -1) {
#if DEBUG
                std::cerr << "Could not connect to shm "
                          << server_info.shm_pathname << ":"
                          << server_info.shm_proj_id << std::endl;
#endif
                server_status.shmid = -1;
            } else {
                server_status.shmid = shmid;
            }
            servers.push_back(server_status);
        }
    }

    closedir(dir);

    // order the servers
    std::sort(servers.begin(), servers.end(), compare);
    return servers_count;
}

void
get_message_from_socket(const live_server_info &server, std::string &msg) {
    const int BUFFER_SIZE = SHARED_MEMORY_SIZE;
    std::vector<char> buffer(BUFFER_SIZE);

    ssize_t bytesRead = read(server.client_fd, buffer.data(), BUFFER_SIZE);
    if (bytesRead == -1) {
#if DEBUG
        std::cerr << "Error occurred while reading from socket" << std::endl;
#endif
        return;
    } else if (bytesRead == 0) {
#if DEBUG
        std::cerr << "Connection closed by the peer" << std::endl;
#endif
        return;
    }

    msg.assign(buffer.data(), bytesRead);
}

void get_message_from_shm(const live_server_info &server, std::string &msg) {
    // Attach the shared memory segment to the process
    void *shmaddr = shmat(server.shmid, nullptr, 0);
    if (shmaddr == reinterpret_cast<void *>(-1)) {
#if DEBUG
        std::cerr << "Failed to attach shared memory segment" << std::endl;
#endif
        return;
    }

    // Read the values from shared memory and copy them to the msg argument
    std::string sharedData(static_cast<char *>(shmaddr));
    msg = sharedData;

    // Detach the shared memory segment from the process
    if (shmdt(shmaddr) == -1) {
#if DEBUG
        std::cerr << "Failed to detach shared memory segment" << std::endl;
#endif
        return;
    }

#if DEBUG
//    std::cout << "Read from shared memory: " << msg << std::endl;
#endif
}

void print_server_infos(const std::vector<live_server_info> &servers) {
    int vm = 0;
    int host = 0;
    int container = 0;
    for (const live_server_info &server: servers) {
        if (server.client_fd == -1 && server.shmid == -1) {
            vm++;
        }else if (server.client_fd != -1 && server.shmid == -1) {
            container++;
        } else if (server.client_fd != -1 && server.shmid != -1) {
            host++;
        }
    }
    std::cout << "Total Servers: " << servers.size() << std::endl;
    std::cout << "Host: " << host << std::endl;
    std::cout << "Container: " << container << std::endl;
    std::cout << "VM: " << vm << std::endl;
    std::cout << "Messages: ";
    std::string shm_msg;
    std::string socket_msg;
    for (live_server_info server: servers) {
        if (server.shmid != -1) { // TODO: change format
            get_message_from_shm(server, shm_msg);
            std::cout << shm_msg << " ";
        }
        if (server.client_fd != -1) {
            get_message_from_socket(server, socket_msg);
            std::cout << socket_msg << " ";
        }

    }
    std::cout << std::endl;
}


void disconnect(const std::vector<live_server_info> &servers) {
    for (int i = 0; i < servers.size(); i++) {
        if (servers[i].client_fd != -1 && close(servers[i].client_fd) == -1) {
#if DEBUG
            std::cerr << "Failed to close server socket" << std::endl;
#endif
            exit_func("Failed to close server socket");
        }
    }
}

void run(const std::string &client_files_directory) {
    std::vector<live_server_info> servers;
    int servers_amount = count_servers(client_files_directory, servers);
    if (servers_amount > 0) {
        print_server_infos(servers);
    }
    disconnect(servers);
}