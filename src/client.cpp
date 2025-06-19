/**
 * @file client.cpp
 * @brief Regular client for Code Compiler & Executor Server
 * @author Rares-Nicholas Popa & Adrian-Petru Enache
 * @version 1.0.0
 *
 * @details This application allows users to submit C source code to the
 * Code Compiler & Executor Server for compilation and execution.
 * It supports both interactive code entry and file loading.
 *
 * @copyright This project is for educational purposes as part of the PCD
 * course.
 */

#include <arpa/inet.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

/** @def SERVER_IP
 * @brief Default server IP address
 */
#define SERVER_IP "127.0.0.1"

/** @def PORT
 * @brief Regular client server port (must match server configuration)
 */
#define PORT 8080

/** @def BUFFER_SIZE
 * @brief Maximum buffer size for network communications
 */
#define BUFFER_SIZE 4096

/**
 * @class RegularClient
 * @brief Client class for code submission and execution
 *
 * This class provides an interface for users to submit C source code
 * to the server for compilation and execution. It supports both
 * interactive code entry and loading code from files.
 */

class RegularClient {
private:
  int sock;                     /**< Socket file descriptor */
  struct sockaddr_in serv_addr; /**< Server address structure */

public:
  /**
   * @brief Default constructor
   *
   * Initializes the RegularClient with invalid socket descriptor.
   */
  RegularClient() : sock(-1) {}

  bool connect_to_server() {
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      std::cerr << "Socket creation error" << std::endl;
      return false;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
      std::cerr << "Invalid address/ Address not supported" << std::endl;
      return false;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
      std::cerr << "Connection Failed" << std::endl;
      return false;
    }

    std::cout << "Connected to server on port " << PORT << std::endl;
    return true;
  }

  /**
   * @brief Send C source code to server for compilation and execution
   *
   * Sends the provided C source code to the server and displays
   * the compilation and execution results.
   *
   * @param code The C source code to compile and execute
   *
   * @details The function:
   * 1. Sends code to server via TCP socket
   * 2. Waits for server response
   * 3. Displays formatted execution results
   * 4. Handles network errors gracefully
   */
  void send_code(const std::string &code) {
    if (send(sock, code.c_str(), code.length(), 0) < 0) {
      std::cerr << "Send failed" << std::endl;
      return;
    }

    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received > 0) {
      std::cout << "\n=== EXECUTION RESULT ===" << std::endl;
      std::cout << buffer << std::endl;
      std::cout << "======================" << std::endl;
    }
  }

  /**
   * @brief Main client loop
   *
   * Provides interactive interface for code submission.
   * Supports both interactive code entry and file loading.
   *
   * @details The function:
   * 1. Connects to the server
   * 2. Displays available commands
   * 3. Processes user input:
   *    - "quit": Exit the client
   *    - "load <filename>": Load and send code from file
   *    - Default: Interactive multi-line code entry
   * 4. Handles file loading errors
   * 5. Provides multi-line code input (end with "END")
   */
  void run() {
    if (!connect_to_server()) {
      return;
    }

    std::cout << "\nRegular Client - Code Compiler & Executor" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "1. Type C code directly (end with 'END' on a new line)"
              << std::endl;
    std::cout << "2. 'load <filename>' - Load code from file" << std::endl;
    std::cout << "3. 'quit' - Exit" << std::endl;

    std::string input;
    while (true) {
      std::cout << "\nClient> ";
      std::getline(std::cin, input);

      if (input == "quit") {
        send(sock, "QUIT", 4, 0);
        break;
      }

      if (input.substr(0, 5) == "load ") {
        std::string filename = input.substr(5);
        std::ifstream file(filename);
        if (file.is_open()) {
          std::stringstream buffer;
          buffer << file.rdbuf();
          std::string code = buffer.str();
          file.close();

          std::cout << "Sending code from file: " << filename << std::endl;
          send_code(code);
        } else {
          std::cout << "Error: Cannot open file " << filename << std::endl;
        }
        continue;
      }

      // Multi-line code input
      std::cout << "Enter your C code (type 'END' on a new line to finish):"
                << std::endl;
      std::stringstream code_stream;
      std::string line;

      while (std::getline(std::cin, line)) {
        if (line == "END") {
          break;
        }
        code_stream << line << "\n";
      }

      std::string code = code_stream.str();
      if (!code.empty()) {
        send_code(code);
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
  ~RegularClient() {
    if (sock >= 0) {
      close(sock);
    }
  }
};

/**
 * @brief Main function for regular client
 *
 * Entry point for the regular client application.
 * Creates a RegularClient instance and runs the interactive interface.
 *
 * @return 0 on successful termination
 */
int main() {
  std::cout << "Code Compiler & Executor - Regular Client" << std::endl;
  std::cout << "=========================================" << std::endl;

  RegularClient client;
  client.run();

  return 0;
}