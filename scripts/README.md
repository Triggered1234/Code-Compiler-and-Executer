# Code Compiler & Executer

A client-server application for remote code compilation and execution.

## Team
- Rares-Nicholas Popa
- Adrian-Petru Enache

## Project Overview

This project implements a client-server system that allows users to submit source code files to a server for compilation and execution. The system supports multiple programming languages and provides both administrative and regular client interfaces.

## Quick Start

1. **Setup environment:**
   ```bash
   ./setup.sh
   ```

2. **Build project:**
   ```bash
   ./build.sh -t Debug -c -T
   ```

3. **Start development environment:**
   ```bash
   cd build
   ./quick_start.sh
   ```

4. **Test with sample code:**
   ```bash
   ./bin/code_client --file test_samples/hello.c
   ```

## Architecture

- **Server (C)**: Main compilation and execution engine
- **Admin Client (C++)**: Administrative interface with UNIX socket
- **Unix Client (C++)**: Regular client for UNIX/Linux systems  
- **Python Client**: Cross-platform client implementation

## Features

- Multi-language support (C, C++, Python, Java, JavaScript, Go)
- Secure compilation environment
- Real-time job monitoring
- Administrative controls
- Async and sync communication modes
- File transfer capabilities

## Documentation

- [Build Instructions](docs/BUILD.md)
- [API Documentation](docs/API.md)
- [User Manual](docs/USER_MANUAL.md)
- [Development Guide](docs/DEVELOPMENT.md)

## License

This project is developed for educational purposes as part of the PCD course.
