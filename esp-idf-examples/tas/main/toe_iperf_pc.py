#!/usr/bin/env python3
import socket
import time
import argparse
import sys

def run_server(port, is_udp=False):
    sock_type = socket.SOCK_DGRAM if is_udp else socket.SOCK_STREAM
    proto_name = "UDP" if is_udp else "TCP"
    print(f"[*] Starting {proto_name} Server on port {port}...")
    
    server_socket = socket.socket(socket.AF_INET, sock_type)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server_socket.bind(('', port))
        
        if not is_udp:
            server_socket.listen(1)
            print(f"[*] Listening for ESP32 {proto_name} connection...")
            conn, addr = server_socket.accept()
            print(f"[+] Connection accepted from {addr}")
        else:
            print(f"[*] Waiting for ESP32 {proto_name} packets...")
            conn = server_socket
            
        total_bytes = 0
        start_time = 0
        
        print("[*] Receiving data... (Test finishes after 2 seconds of silence)")
        
        last_rx_time = time.time()
        while True:
            try:
                server_socket.settimeout(2.0) # 2 seconds timeout to detect end of test
                if is_udp:
                    data, addr = conn.recvfrom(8192)
                else:
                    data = conn.recv(8192)
                    
                if not data:
                    if not is_udp: break # TCP connection closed
                    
                if total_bytes == 0:
                    start_time = time.time()
                    print(f"[+] Started receiving data from {addr if is_udp else addr}...")
                    
                total_bytes += len(data)
                last_rx_time = time.time()
                
            except socket.timeout:
                if total_bytes > 0:
                    break # test finished
                else:
                    continue # keep waiting for first packet
            except Exception as e:
                break
                
        duration = last_rx_time - start_time if start_time > 0 else 0
        mbps = (total_bytes * 8) / (duration * 1000000) if duration > 0 else 0
        
        print(f"\n--- PC Rx (ESP32 Tx) Report ({proto_name}) ---")
        print(f"Duration   : {duration:.2f} s")
        print(f"Transferred: {total_bytes / (1024*1024):.2f} MB")
        print(f"Bandwidth  : {mbps:.2f} Mbps")
        print("---------------------------------------")
        
        if not is_udp:
            conn.close()
    except Exception as e:
        print(f"[-] Error: {e}")
    finally:
        server_socket.close()

def run_client(ip, port, is_udp=False):
    sock_type = socket.SOCK_DGRAM if is_udp else socket.SOCK_STREAM
    proto_name = "UDP" if is_udp else "TCP"
    print(f"[*] Connecting to ESP32 at {ip}:{port} via {proto_name}...")
    
    client_socket = socket.socket(socket.AF_INET, sock_type)
    
    try:
        if not is_udp:
            client_socket.connect((ip, port))
            print("[+] Connected!")
        
        # 8KB buffer of dummy data (1472 bytes is better for UDP to avoid IP fragmentation)
        chunk_size = 1472 if is_udp else 8192
        dummy_data = b'\xAB' * chunk_size
        
        start_time = time.time()
        total_bytes = 0
        
        print(f"[*] Sending dummy data for 10 seconds...")
        
        while time.time() - start_time < 10.0:
            if is_udp:
                bytes_sent = client_socket.sendto(dummy_data, (ip, port))
                # Small sleep for UDP to avoid overwhelming local network buffer instantly
                time.sleep(0.001) 
            else:
                bytes_sent = client_socket.send(dummy_data)
            total_bytes += bytes_sent
            
        duration = time.time() - start_time
        mbps = (total_bytes * 8) / (duration * 1000000)
        
        print(f"\n--- PC Tx (ESP32 Rx) Report ({proto_name}) ---")
        print(f"Duration   : {duration:.2f} s")
        print(f"Transferred: {total_bytes / (1024*1024):.2f} MB")
        print(f"Bandwidth  : {mbps:.2f} Mbps")
        print("---------------------------------------")
        
    except KeyboardInterrupt:
        print("\n[-] Test stopped by user.")
    except Exception as e:
        print(f"[-] Error: {e}")
    finally:
        client_socket.close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="PC counterpart for ESP32 TOE Throughput Test")
    parser.add_argument('-s', '--server', action='store_true', help="Run in Server mode (Test ESP32 Tx)")
    parser.add_argument('-c', '--client', type=str, metavar='IP', help="Run in Client mode (Test ESP32 Rx) to the specified IP")
    parser.add_argument('-p', '--port', type=int, default=5003, help="Port to use (default: 5003)")
    parser.add_argument('-u', '--udp', action='store_true', help="Use UDP instead of TCP")
    
    args = parser.parse_args()
    
    if args.server:
        run_server(args.port, args.udp)
    elif args.client:
        run_client(args.client, args.port, args.udp)
    else:
        parser.print_help()
        sys.exit(1)
