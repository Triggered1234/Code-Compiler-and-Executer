"""
Communication module for Code Compiler & Executer Python Client
Handles protocol messages and network communication
Authors: Rares-Nicholas Popa & Adrian-Petru Enache
Date: April 2025
"""

import socket
import struct
import time
import threading
import hashlib
from typing import Optional, Dict, Any, Tuple
from enum import IntEnum


class MessageType(IntEnum):
    """Protocol message types"""
    # Client messages
    MSG_HELLO = 1
    MSG_FILE_UPLOAD_START = 2
    MSG_FILE_UPLOAD_CHUNK = 3
    MSG_FILE_UPLOAD_END = 4
    MSG_COMPILE_REQUEST = 5
    MSG_STATUS_REQUEST = 6
    MSG_RESULT_REQUEST = 7
    MSG_PING = 8
    
    # Server responses
    MSG_ACK = 100
    MSG_NACK = 101
    MSG_ERROR = 102
    MSG_COMPILE_RESPONSE = 103
    MSG_STATUS_RESPONSE = 104
    MSG_RESULT_RESPONSE = 105
    MSG_PONG = 106


class ProtocolError(Exception):
    """Protocol-related errors"""
    pass


class Message:
    """Protocol message structure"""
    
    MAGIC = 0x43434545  # "CCEE"
    HEADER_SIZE = 32
    
    def __init__(self, msg_type: MessageType, data: bytes = b'', correlation_id: int = 0):
        self.magic = self.MAGIC
        self.message_type = msg_type
        self.flags = 0
        self.data_length = len(data) if data else 0
        self.correlation_id = correlation_id
        self.timestamp = int(time.time() * 1000)  # milliseconds
        self.checksum = 0
        self.data = data
        
        # Calculate checksum
        self.checksum = self._calculate_checksum()
    
    def _calculate_checksum(self) -> int:
        """Calculate header checksum"""
        # Pack header without checksum
        header_data = struct.pack('>IHHIIQ', 
                                 self.magic,
                                 self.message_type,
                                 self.flags,
                                 self.data_length,
                                 self.correlation_id,
                                 self.timestamp)
        
        # Simple CRC32 checksum
        import zlib
        return zlib.crc32(header_data) & 0xffffffff
    
    def pack(self) -> bytes:
        """Pack message into binary format"""
        header = struct.pack('>IHHIIQI', 
                           self.magic,
                           self.message_type,
                           self.flags,
                           self.data_length,
                           self.correlation_id,
                           self.timestamp,
                           self.checksum)
        
        return header + self.data
    
    @classmethod
    def unpack(cls, data: bytes) -> 'Message':
        """Unpack message from binary format"""
        if len(data) < cls.HEADER_SIZE:
            raise ProtocolError("Incomplete message header")
        
        # Unpack header
        header = struct.unpack('>IHHIIQI', data[:cls.HEADER_SIZE])
        magic, msg_type, flags, data_length, correlation_id, timestamp, checksum = header
        
        # Validate magic number
        if magic != cls.MAGIC:
            raise ProtocolError(f"Invalid magic number: {magic:08x}")
        
        # Validate message type
        try:
            msg_type = MessageType(msg_type)
        except ValueError:
            raise ProtocolError(f"Invalid message type: {msg_type}")
        
        # Extract message data
        if data_length > 0:
            if len(data) < cls.HEADER_SIZE + data_length:
                raise ProtocolError("Incomplete message data")
            msg_data = data[cls.HEADER_SIZE:cls.HEADER_SIZE + data_length]
        else:
            msg_data = b''
        
        # Create message and validate checksum
        msg = cls(msg_type, msg_data, correlation_id)
        msg.flags = flags
        msg.timestamp = timestamp
        
        if msg.checksum != checksum:
            raise ProtocolError("Message checksum mismatch")
        
        return msg


class MessageHandler:
    """Handles message sending and receiving"""
    
    def __init__(self, sock: socket.socket, verbose: bool = False):
        self.socket = sock
        self.verbose = verbose
        self._correlation_counter = 0
        self._lock = threading.Lock()
    
    def _next_correlation_id(self) -> int:
        """Generate next correlation ID"""
        with self._lock:
            self._correlation_counter += 1
            return self._correlation_counter
    
    def send_message(self, msg: Message) -> bool:
        """Send a message"""
        try:
            data = msg.pack()
            
            if self.verbose:
                print(f"Sending {msg.message_type.name} ({len(data)} bytes)")
            
            sent = 0
            while sent < len(data):
                n = self.socket.send(data[sent:])
                if n == 0:
                    raise ProtocolError("Socket connection broken")
                sent += n
            
            return True
            
        except Exception as e:
            if self.verbose:
                print(f"Send error: {e}")
            return False
    
    def receive_message(self, timeout: Optional[float] = None) -> Optional[Message]:
        """Receive a message"""
        try:
            # Set timeout if specified
            old_timeout = self.socket.gettimeout()
            if timeout is not None:
                self.socket.settimeout(timeout)
            
            try:
                # Receive header
                header_data = self._recv_exact(Message.HEADER_SIZE)
                if not header_data:
                    return None
                
                # Parse header to get data length
                header = struct.unpack('>IHHIIQI', header_data)
                data_length = header[3]
                
                # Receive data if present
                if data_length > 0:
                    msg_data = self._recv_exact(data_length)
                    if not msg_data:
                        return None
                    full_data = header_data + msg_data
                else:
                    full_data = header_data
                
                # Unpack message
                msg = Message.unpack(full_data)
                
                if self.verbose:
                    print(f"Received {msg.message_type.name} ({len(full_data)} bytes)")
                
                return msg
                
            finally:
                self.socket.settimeout(old_timeout)
            
        except Exception as e:
            if self.verbose:
                print(f"Receive error: {e}")
            return None
    
    def _recv_exact(self, size: int) -> Optional[bytes]:
        """Receive exact number of bytes"""
        data = b''
        while len(data) < size:
            chunk = self.socket.recv(size - len(data))
            if not chunk:
                return None
            data += chunk
        return data
    
    def send_and_receive(self, msg: Message, timeout: float = 30.0) -> Optional[Message]:
        """Send a message and wait for response"""
        if not self.send_message(msg):
            return None
        
        return self.receive_message(timeout)
    
    def send_hello(self, hello_data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Send hello message"""
        # Pack hello data
        data = struct.pack('>HHH H 64s 32s',
                          hello_data.get('client_version_major', 1),
                          hello_data.get('client_version_minor', 0),
                          hello_data.get('client_version_patch', 0),
                          hello_data.get('capabilities', 0),
                          hello_data.get('client_name', 'Python Client').encode('utf-8')[:64],
                          hello_data.get('client_platform', 'Python').encode('utf-8')[:32])
        
        msg = Message(MessageType.MSG_HELLO, data, self._next_correlation_id())
        response = self.send_and_receive(msg)
        
        if response and response.message_type == MessageType.MSG_HELLO:
            # Unpack hello response
            if len(response.data) >= 102:  # Minimum hello response size
                unpacked = struct.unpack('>HHH H 64s 32s', response.data[:102])
                return {
                    'status': 'ok',
                    'server_version_major': unpacked[0],
                    'server_version_minor': unpacked[1],
                    'server_version_patch': unpacked[2],
                    'capabilities': unpacked[3],
                    'server_name': unpacked[4].decode('utf-8').rstrip('\x00'),
                    'server_platform': unpacked[5].decode('utf-8').rstrip('\x00')
                }
        
        return None
    
    def send_compile_request(self, compile_data: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Send compile request"""
        # Pack compile request data
        filename = compile_data.get('filename', '').encode('utf-8')[:256]
        compiler_args = compile_data.get('compiler_args', '').encode('utf-8')[:1024]
        execution_args = compile_data.get('execution_args', '').encode('utf-8')[:1024]
        
        data = struct.pack('>HHHH 256s 1024s 1024s',
                          compile_data.get('language', 0),
                          compile_data.get('mode', 0),
                          compile_data.get('flags', 0),
                          compile_data.get('priority', 5),
                          filename,
                          compiler_args,
                          execution_args)
        
        msg = Message(MessageType.MSG_COMPILE_REQUEST, data, self._next_correlation_id())
        response = self.send_and_receive(msg)
        
        if response and response.message_type == MessageType.MSG_COMPILE_RESPONSE:
            if len(response.data) >= 20:  # Minimum compile response size
                unpacked = struct.unpack('>I H H i I I I', response.data[:20])
                return {
                    'job_id': unpacked[0],
                    'status': unpacked[1],
                    'exit_code': unpacked[3],
                    'output_size': unpacked[4],
                    'error_size': unpacked[5],
                    'execution_time_ms': unpacked[6]
                }
        elif response and response.message_type == MessageType.MSG_ERROR:
            return self._parse_error_response(response)
        
        return None
    
    def send_status_request(self, job_id: int) -> Optional[Dict[str, Any]]:
        """Send status request"""
        data = struct.pack('>I', job_id)
        msg = Message(MessageType.MSG_STATUS_REQUEST, data, self._next_correlation_id())
        response = self.send_and_receive(msg)
        
        if response and response.message_type == MessageType.MSG_STATUS_RESPONSE:
            if len(response.data) >= 24:  # Minimum status response size
                unpacked = struct.unpack('>I H H Q Q i', response.data[:24])
                result = {
                    'job_id': unpacked[0],
                    'status': unpacked[1],
                    'progress': unpacked[2],
                    'start_time': unpacked[3],
                    'end_time': unpacked[4],
                    'pid': unpacked[5]
                }
                
                # Extract status message if present
                if len(response.data) > 24:
                    status_msg = response.data[24:].decode('utf-8').rstrip('\x00')
                    result['status_message'] = status_msg
                
                return result
        elif response and response.message_type == MessageType.MSG_ERROR:
            return self._parse_error_response(response)
        
        return None
    
    def send_result_request(self, job_id: int) -> Optional[Dict[str, Any]]:
        """Send result request"""
        data = struct.pack('>I', job_id)
        msg = Message(MessageType.MSG_RESULT_REQUEST, data, self._next_correlation_id())
        response = self.send_and_receive(msg)
        
        if response and response.message_type == MessageType.MSG_RESULT_RESPONSE:
            if len(response.data) >= 20:
                unpacked = struct.unpack('>I H H i I I I', response.data[:20])
                result = {
                    'job_id': unpacked[0],
                    'status': unpacked[1],
                    'exit_code': unpacked[3],
                    'output_size': unpacked[4],
                    'error_size': unpacked[5],
                    'execution_time_ms': unpacked[6]
                }
                
                # For now, we don't handle actual output/error data transfer
                # In a complete implementation, this would involve additional messages
                result['output'] = f"[Output size: {result['output_size']} bytes]"
                result['error'] = f"[Error size: {result['error_size']} bytes]"
                
                return result
        elif response and response.message_type == MessageType.MSG_ERROR:
            return self._parse_error_response(response)
        
        return None
    
    def send_ping(self) -> Optional[Dict[str, Any]]:
        """Send ping message"""
        msg = Message(MessageType.MSG_PING, b'', self._next_correlation_id())
        response = self.send_and_receive(msg, timeout=5.0)
        
        if response and response.message_type == MessageType.MSG_PONG:
            return {'status': 'ok'}
        
        return None
    
    def _parse_error_response(self, response: Message) -> Dict[str, Any]:
        """Parse error response"""
        if len(response.data) >= 8:
            error_code, error_line = struct.unpack('>I I', response.data[:8])
            
            # Extract error message
            error_message = ""
            error_context = ""
            
            if len(response.data) > 8:
                remaining = response.data[8:]
                # Find null terminator for error message
                null_pos = remaining.find(b'\x00')
                if null_pos != -1:
                    error_message = remaining[:null_pos].decode('utf-8', errors='ignore')
                    if len(remaining) > null_pos + 1:
                        context_data = remaining[null_pos + 1:]
                        context_null = context_data.find(b'\x00')
                        if context_null != -1:
                            error_context = context_data[:context_null].decode('utf-8', errors='ignore')
            
            return {
                'status': 'error',
                'error_code': error_code,
                'error_line': error_line,
                'error_message': error_message,
                'error_context': error_context
            }
        
        return {'status': 'error', 'error_message': 'Unknown error'}


class AsyncMessageHandler:
    """Asynchronous message handler for non-blocking operations"""
    
    def __init__(self, message_handler: MessageHandler):
        self.message_handler = message_handler
        self._pending_requests = {}
        self._lock = threading.Lock()
        self._running = False
        self._thread = None
    
    def start(self):
        """Start async message handling"""
        if self._running:
            return
        
        self._running = True
        self._thread = threading.Thread(target=self._message_loop, daemon=True)
        self._thread.start()
    
    def stop(self):
        """Stop async message handling"""
        self._running = False
        if self._thread:
            self._thread.join(timeout=1.0)
    
    def _message_loop(self):
        """Message handling loop"""
        while self._running:
            try:
                msg = self.message_handler.receive_message(timeout=1.0)
                if msg:
                    self._handle_response(msg)
            except Exception as e:
                if self.message_handler.verbose:
                    print(f"Async message loop error: {e}")
    
    def _handle_response(self, msg: Message):
        """Handle received response"""
        with self._lock:
            correlation_id = msg.correlation_id
            if correlation_id in self._pending_requests:
                event, result = self._pending_requests[correlation_id]
                result['response'] = msg
                event.set()
    
    def send_async(self, msg: Message, timeout: float = 30.0) -> Optional[Message]:
        """Send message asynchronously and wait for response"""
        event = threading.Event()
        result = {}
        
        with self._lock:
            self._pending_requests[msg.correlation_id] = (event, result)
        
        try:
            if not self.message_handler.send_message(msg):
                return None
            
            if event.wait(timeout):
                return result.get('response')
            else:
                return None  # Timeout
                
        finally:
            with self._lock:
                self._pending_requests.pop(msg.correlation_id, None)


# Utility functions
def calculate_file_checksum(filepath: str) -> int:
    """Calculate CRC32 checksum of a file"""
    import zlib
    checksum = 0
    
    with open(filepath, 'rb') as f:
        while True:
            chunk = f.read(8192)
            if not chunk:
                break
            checksum = zlib.crc32(chunk, checksum)
    
    return checksum & 0xffffffff


def validate_message_header(data: bytes) -> bool:
    """Validate message header format"""
    if len(data) < Message.HEADER_SIZE:
        return False
    
    try:
        header = struct.unpack('>IHHIIQI', data[:Message.HEADER_SIZE])
        magic = header[0]
        return magic == Message.MAGIC
    except:
        return False


def get_error_name(error_code: int) -> str:
    """Get human-readable error name"""
    error_names = {
        0: "No Error",
        1: "Invalid Argument",
        2: "Permission Denied",
        3: "Not Found",
        4: "Quota Exceeded",
        5: "Memory Allocation Error",
        6: "Internal Error",
        7: "Timeout",
        8: "Compilation Error",
        9: "Execution Error",
        10: "Network Error",
        11: "File I/O Error",
        12: "Unsupported Language"
    }
    
    return error_names.get(error_code, f"Unknown Error ({error_code})")