/**
 * @file admin_client.cpp
 * @brief Administrative client for Code Compiler & Executor Server
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @version 1.0.0
 *
 * @details This application provides administrative interface for managing
 * the Code Compiler & Executor Server. It connects to the admin port (8081)
 * and allows monitoring server status, viewing logs, and shutting down the
 * server.
 *
 * @copyright This project is for educational purposes as part of the PCD
 * course.
 */

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

/** @def SERVER_IP
 * @brief Default server IP address
 */
#define SERVER_IP "127.0.0.1"

/** @def ADMIN_PORT
 * @brief Admin server port (must match server configuration)
 */
#define ADMIN_PORT 8081

/** @def BUFFER_SIZE
 * @brief Maximum buffer size for network communications
 */
#define BUFFER_SIZE 4096

/**
 * @class AdminClient
 * @brief Administrative client class for server management
 *
 * This class provides an interface for administrators to connect to the
 * Code Compiler & Executor Server and perform management tasks such as
 * monitoring status, viewing logs, and shutting down the server.
 */

class AdminClient {
private:
  int sock;                     /**< Socket file descriptor */
  struct sockaddr_in serv_addr; /**< Server address structure */

public:
  /**
   * @brief Default constructor
   *
   * Initializes the AdminClient with invalid socket descriptor.
   */
  AdminClient() : sock(-1) {}

  bool connect_to_server() {
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      std::cerr << "Socket creation error" << std::endl;
      return false;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(ADMIN_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
      std::cerr << "Invalid address/ Address not supported" << std::endl;
      return false;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      std::cerr << "Connection Failed" << std::endl;
      return false;
    }

    std::cout << "Connected to admin server on port " << ADMIN_PORT
              << std::endl;
    return true;
  }

  /**
   * @brief Send administrative command to server
   *
   * Sends a command to the server and receives the response.
   *
   * @param command The administrative command to send
   *
   * @details Supported commands:
   * - "STATUS": Get server statistics
   * - "LOGS": View server logs
   * - "SHUTDOWN": Shutdown server
   * - "QUIT": Disconnect from server
   */
  void send_command(const std::string &command) {
    if (send(sock, command.c_str(), command.length(), 0) < 0) {
      std::cerr << "Send failed" << std::endl;
      return;
    }

    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received > 0) {
      std::cout << "Server response:\n" << buffer << std::endl;
    }
  }

  /**
   * @brief Main client loop
   *
   * Connects to server and provides interactive command interface.
   * Displays available commands and processes user input until
   * the user chooses to exit or disconnect.
   *
   * @details The function:
   * 1. Connects to the admin server
   * 2. Displays available commands
   * 3. Processes user input in a loop
   * 4. Handles special commands (exit, QUIT, SHUTDOWN)
   * 5. Provides feedback for unknown commands
   */
  void run() {
    if (!connect_to_server()) {
      return;
    }

    std::string command;
    std::cout << "\nAdmin Client - Available commands:" << std::endl;
    std::cout << "STATUS  - Get server statistics" << std::endl;
    std::cout << "LOGS    - View server logs" << std::endl;
    std::cout << "SHUTDOWN- Shutdown the server" << std::endl;
    std::cout << "QUIT    - Disconnect from server" << std::endl;
    std::cout << "exit    - Exit this client" << std::endl;

    while (true) {
      std::cout << "\nAdmin> ";
      std::getline(std::cin, command);

      if (command == "exit") {
        break;
      }

      if (command == "QUIT") {
        send_command(command);
        break;
      }

      if (command == "STATUS" || command == "LOGS" || command == "SHUTDOWN") {
        send_command(command);

        if (command == "SHUTDOWN") {
          std::cout << "Server shutdown initiated." << std::endl;
          break;
        }
      } else if (command.empty()) {
        continue;
      } else {
        std::cout
            << "Unknown command. Available: STATUS, LOGS, SHUTDOWN, QUIT, exit"
            << std::endl;
      }
    }

    close(sock);
    std::cout << "Disconnected from server." << std::endl;
  }

  /**
   * @brief Destructor
   *
   * Ensures socket is properly closed when object is destroyed.
   */
  ~AdminClient() {
    if (sock >= 0) {
      close(sock);
    }
  }
};

/**
 * @brief Main function for admin client
 *
 * Entry point for the administrative client application.
 * Creates an AdminClient instance and runs the interactive interface.
 *
 * @return 0 on successful termination
 */
int main() {
  std::cout << "Code Compiler & Executor - Admin Client" << std::endl;
  std::cout << "=======================================" << std::endl;

  AdminClient client;
  client.run();

  return 0;
}