import socket
import struct
import threading
import time
import json
import random
import argparse
import sys

# Configuration
CHAT_SERVER_HOST = '127.0.0.1'
CHAT_SERVER_PORT = 8090
GATE_SERVER_HOST = '127.0.0.1'
GATE_SERVER_PORT = 8080

MSG_CHAT_LOGIN = 1005

# Statistics
success_count = 0
fail_count = 0
lock = threading.Lock()

def test_gate_server(thread_id):
    """Test GateServer connection (HTTP)"""
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.settimeout(5) # Increased timeout
        client.connect((GATE_SERVER_HOST, GATE_SERVER_PORT))
        request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        client.send(request.encode())
        response = client.recv(4096)
        client.close()
        return True
    except Exception as e:
        # print(f"[Thread {thread_id}] GateServer connection failed: {e}")
        return False

def test_chat_server(thread_id):
    """Test ChatServer connection (TCP)"""
    global success_count, fail_count
    try:
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.settimeout(5) # Increased timeout
        client.connect((CHAT_SERVER_HOST, CHAT_SERVER_PORT))
        
        # Prepare Login Data
        data = {
            "uid": random.randint(1000, 20000), # Increased range
            "token": "test_token"
        }
        json_str = json.dumps(data)
        body = json_str.encode('utf-8')
        
        # Pack Header: MsgID (2 bytes) + Length (2 bytes)
        # Network byte order (!)
        header = struct.pack('!HH', MSG_CHAT_LOGIN, len(body))
        
        # Send
        client.sendall(header + body)
        
        # Receive Response Header
        resp_header = client.recv(4)
        if len(resp_header) < 4:
            with lock:
                fail_count += 1
            return False
            
        resp_msg_id, resp_len = struct.unpack('!HH', resp_header)
        
        # Receive Body
        resp_body = client.recv(resp_len)
        
        client.close()
        with lock:
            success_count += 1
        return True
    except Exception as e:
        # print(f"[Thread {thread_id}] ChatServer connection failed: {e}")
        with lock:
            fail_count += 1
        return False

def worker(start_index, count):
    for i in range(count):
        test_chat_server(start_index + i)
        # test_gate_server(start_index + i) # Focus on ChatServer for C10K

def run_stress_test(num_connections=1000, num_threads=100):
    print(f"Starting stress test with {num_connections} connections using {num_threads} threads...")
    
    start_time = time.time()
    
    threads = []
    conns_per_thread = num_connections // num_threads
    
    for i in range(num_threads):
        t = threading.Thread(target=worker, args=(i * conns_per_thread, conns_per_thread))
        threads.append(t)
        t.start()
        
    for t in threads:
        t.join()
        
    end_time = time.time()
    duration = end_time - start_time
    print(f"Stress test finished in {duration:.2f} seconds.")
    print(f"Success: {success_count}, Failed: {fail_count}")
    if duration > 0:
        print(f"Requests per second (RPS): {success_count / duration:.2f}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Stress Test for ChatServer')
    parser.add_argument('-c', '--connections', type=int, default=1000, help='Total number of connections')
    parser.add_argument('-t', '--threads', type=int, default=100, help='Number of threads')
    args = parser.parse_args()
    
    run_stress_test(args.connections, args.threads)
