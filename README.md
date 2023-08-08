# IPC Communication Library          </a> <a href="https://www.cprogramming.com/" target="_blank" rel="noreferrer"> <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/c/c-original.svg" alt="c" width="40" height="40"/> </a> <a href="https://www.w3schools.com/cpp/" target="_blank" rel="noreferrer"> <img src="https://raw.githubusercontent.com/devicons/devicon/master/icons/cplusplus/cplusplus-original.svg" alt="cplusplus" width="40" height="40"/>

This repository contains a C++ communication library that enables efficient communication between client programs and multiple servers using both sockets and shared memory. The library is designed to facilitate seamless interaction between distributed systems, allowing clients to send and receive messages using various communication modes.

## Overview

The Hierarchical Virtual Memory Communication Library consists of two main components: `client.cpp` and `server.cpp`. These components work together to establish communication channels, exchange messages, and manage shared memory segments.

### Client (`client.cpp`)

The `client.cpp` file implements the client-side communication logic. It reads server information from files located in a specified directory and establishes connections to servers using sockets. Additionally, it connects to shared memory segments associated with the servers. The client retrieves messages from both sockets and shared memory, processes the received data, and provides information about the communication setup.

### Server (`server.cpp`)

The `server.cpp` file implements the server-side communication logic. It sets up a server socket to accept incoming connections from clients. The server also connects to shared memory segments and writes relevant information to a file. The `run` function within the file manages sending messages to clients via sockets and shared memory. After a brief operation, the server gracefully shuts down.

## Usage Instructions

Follow these steps to integrate and utilize the Hierarchical Virtual Memory Communication Library in your project:

1. **Compile the Library Components:**
   - Use the provided `makefile` to compile the library components.
   - Execute the command `make` in the terminal to compile the static libraries `libclient.a` and `libserver.a`.

2. **Integrate Libraries into Your Project:**
   - Link the compiled libraries (`libclient.a` and `libserver.a`) to your project to access client and server functionalities.

3. **Client Integration:**
   - Include the `client.h` header in your client program.
   - Implement your logic to specify the directory containing server information files.
   - Invoke the `run` function from `client.cpp` to initiate client communication.
   - The `run` function retrieves messages from sockets and shared memory, categorizes servers, and displays received messages.

4. **Server Integration:**
   - Include the `client.h` header in your server program.
   - Implement your logic to set up a server socket and establish connections with clients.
   - Connect to shared memory segments and write necessary information to a file.
   - Utilize the `run` function from `server.cpp` to send messages to clients via sockets and shared memory.

5. **Customization:**
   - Modify the code to match your project requirements, communication protocols, error handling, and message formats.

## Note

This library serves as a foundation for efficient communication between clients and servers using sockets and shared memory. Ensure that you adapt the code to your specific use case, implement proper error handling, and consider security considerations.

Feel free to use the provided makefile as a reference for building your project. Explore additional features, enhancements, and optimizations to meet your unique communication needs. Your contributions to extending and improving this library are highly encouraged.

For more details, technical insights, and code explanations, refer to the source code and comments within the `client.cpp`, `server.cpp`, and other relevant files.

## Contact

For any questions or assistance, feel free to contact the library's developer at `Eitan.Stepanov@gmail.com`.
