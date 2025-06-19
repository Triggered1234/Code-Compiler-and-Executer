# API Documentation

## Protocol Overview

The system uses a custom binary protocol over TCP sockets for client-server communication and UNIX sockets for administration.

## Message Format

All messages follow this structure:
- Magic number (4 bytes)
- Message type (2 bytes)  
- Flags (2 bytes)
- Data length (4 bytes)
- Correlation ID (4 bytes)
- Timestamp (8 bytes)
- Checksum (4 bytes)
- Data payload (variable)

## Message Types

### Client Messages
- `MSG_COMPILE_REQUEST` - Submit code for compilation
- `MSG_STATUS_REQUEST` - Check job status
- `MSG_RESULT_REQUEST` - Retrieve results

### Server Responses
- `MSG_COMPILE_RESPONSE` - Compilation results
- `MSG_STATUS_RESPONSE` - Job status update
- `MSG_ERROR` - Error information

### Admin Messages
- `MSG_ADMIN_LIST_CLIENTS` - List connected clients
- `MSG_ADMIN_LIST_JOBS` - List active jobs
- `MSG_ADMIN_SERVER_STATS` - Get server statistics

## Examples

See the client implementations for usage examples.
