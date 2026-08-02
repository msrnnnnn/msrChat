#!/usr/bin/env python3
"""
msrChat 长连接稳定性压测
测试心跳保活、断线重连、服务端重启恢复。

用法:
  python3 scripts/stress_stability.py
  python3 scripts/stress_stability.py --duration 120
  python3 scripts/stress_stability.py --server /path/to/ChatServer
"""
import argparse, json, os, random, signal, socket, struct, subprocess, sys, threading, time

# ─── 协议常量 ────────────────────────────────────────────────
MSG_HELLO = 1000
MSG_CHAT_LOGIN = 1005
MSG_CHAT_TEXT = 1006
MSG_CHAT_ACK = 1007
HEAD_TOTAL_LEN = 6
SERVER_PORT = 8080

# ─── 协议辅助 ────────────────────────────────────────────────
def build_packet(msg_id: int, body: bytes) -> bytes:
    return struct.pack('!HI', msg_id, len(body)) + body

def recv_n(sock: socket.socket, n: int) -> bytes:
    buf = b''
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf

def recv_packet(sock: socket.socket, timeout: float = 5.0):
    sock.settimeout(timeout)
    header = recv_n(sock, HEAD_TOTAL_LEN)
    if header is None:
        return None, None
    msg_id, body_len = struct.unpack('!HI', header)
    body = recv_n(sock, body_len)
    if body is None:
        return None, None
    return msg_id, body

# ─── 客户端 ──────────────────────────────────────────────────
class StableClient:
    def __init__(self, uid: int, host: str = "127.0.0.1", port: int = SERVER_PORT):
        self.uid = uid
        self.host = host
        self.port = port
        self.sock = None
        self.connected = False
        self.logged_in = False
        self.heartbeats_sent = 0
        self.heartbeats_acked = 0
        self.reconnects = 0
        self.messages_sent = 0
        self.messages_acked = 0
        self.disconnected_by_server = False
        self._stop = threading.Event()

    def connect_and_login(self) -> bool:
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(10.0)
            self.sock.connect((self.host, self.port))
            self.connected = True

            body = json.dumps({"uid": self.uid, "token": "dev_token"}).encode()
            self.sock.sendall(build_packet(MSG_CHAT_LOGIN, body))
            msg_id, rsp_body = recv_packet(self.sock)
            if msg_id == MSG_CHAT_LOGIN:
                rsp = json.loads(rsp_body)
                if rsp.get("error") == 0:
                    self.logged_in = True
                    return True
        except:
            pass
        return False

    def send_heartbeat(self) -> bool:
        try:
            self.sock.sendall(build_packet(MSG_HELLO, b'{}'))
            self.heartbeats_sent += 1
            msg_id, _ = recv_packet(self.sock, timeout=5.0)
            if msg_id == MSG_HELLO:
                self.heartbeats_acked += 1
                return True
        except:
            pass
        return False

    def send_message(self, to_uid: int, content: str) -> bool:
        import uuid
        from struct import pack as struct_pack

        # 简单 protobuf 编码
        buf = bytearray()
        def varint(v):
            while v > 0x7f:
                buf.append((v & 0x7f) | 0x80)
                v >>= 7
            buf.append(v)
        def add_field(tag, wire, val):
            varint((tag << 3) | wire)
            buf.extend(val)

        add_field(1, 0, varint(self.uid))
        add_field(2, 0, varint(to_uid))
        c = content.encode('utf-8')
        add_field(3, 2, varint(len(c)) + c)
        mid = str(uuid.uuid4()).encode('utf-8')
        add_field(4, 2, varint(len(mid)) + mid)

        try:
            self.sock.sendall(build_packet(MSG_CHAT_TEXT, bytes(buf)))
            self.messages_sent += 1
            msg_id, _ = recv_packet(self.sock, timeout=5.0)
            if msg_id == MSG_CHAT_ACK:
                self.messages_acked += 1
                return True
        except:
            pass
        return False

    def reconnect(self) -> bool:
        self.close()
        self.reconnects += 1
        return self.connect_and_login()

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.connected = False
        self.logged_in = False

    def stop(self):
        self._stop.set()

# ─── 服务端管理 ──────────────────────────────────────────────
class ServerHarness:
    def __init__(self, binary: str, build_dir: str):
        self.binary = binary
        self.build_dir = build_dir
        self.proc = None

    def start(self) -> bool:
        log_path = os.path.join(self.build_dir, "stress_stability_server.log")
        log = open(log_path, "w")
        self.proc = subprocess.Popen(
            [self.binary], cwd=self.build_dir,
            stdout=log, stderr=log)
        for _ in range(30):
            time.sleep(0.3)
            try:
                s = socket.socket()
                s.settimeout(0.5)
                s.connect(("127.0.0.1", SERVER_PORT))
                s.close()
                return True
            except (ConnectionRefusedError, OSError):
                continue
        return False

    def stop(self):
        if not self.proc:
            return
        self.proc.send_signal(signal.SIGINT)
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()

    def restart(self) -> bool:
        self.stop()
        time.sleep(1)
        return self.start()

# ─── 压测场景 ────────────────────────────────────────────────
def test_heartbeat_stability(host: str, duration: int, client_count: int) -> dict:
    """场景 1: 多连接保持心跳，验证不掉线"""
    clients = []
    for i in range(client_count):
        c = StableClient(9000 + i, host)
        if c.connect_and_login():
            clients.append(c)
        else:
            c.close()

    if not clients:
        return {"error": "无客户端连接成功"}

    t0 = time.monotonic()
    heartbeat_interval = 5.0
    drop_count = 0

    while time.monotonic() - t0 < duration:
        alive = []
        for c in clients:
            if not c.send_heartbeat():
                drop_count += 1
            else:
                alive.append(c)
        clients = alive
        time.sleep(heartbeat_interval)

    elapsed = time.monotonic() - t0

    for c in clients:
        c.close()

    total_hb = sum(c.heartbeats_sent for c in clients)
    total_hb_ok = sum(c.heartbeats_acked for c in clients)

    return {
        "duration": elapsed,
        "initial_clients": client_count,
        "final_clients": len(clients),
        "drops": drop_count,
        "heartbeat_success_rate": total_hb_ok / total_hb if total_hb > 0 else 0,
        "survival_rate": len(clients) / client_count if client_count > 0 else 0,
    }

def test_reconnect_stability(host: str, duration: int, client_count: int, drop_rate: float = 0.3) -> dict:
    """场景 2: 随机断开连接，验证自动重连"""
    clients = []
    for i in range(client_count):
        c = StableClient(9100 + i, host)
        if c.connect_and_login():
            clients.append(c)
        else:
            c.close()

    if not clients:
        return {"error": "无客户端连接成功"}

    t0 = time.monotonic()
    total_reconnects = 0
    successful_reconnects = 0
    heartbeat_interval = 3.0

    while time.monotonic() - t0 < duration:
        for c in list(clients):
            # 随机断开
            if random.random() < drop_rate:
                c.close()
                total_reconnects += 1
                if c.reconnect():
                    successful_reconnects += 1
            else:
                c.send_heartbeat()
        time.sleep(heartbeat_interval)

    elapsed = time.monotonic() - t0

    for c in clients:
        c.close()

    alive_count = sum(1 for c in clients if c.logged_in)

    return {
        "duration": elapsed,
        "clients": client_count,
        "total_reconnects": total_reconnects,
        "successful_reconnects": successful_reconnects,
        "reconnect_success_rate": successful_reconnects / total_reconnects if total_reconnects > 0 else 0,
        "final_alive": alive_count,
    }

def test_server_restart_recovery(harness: ServerHarness, host: str, client_count: int) -> dict:
    """场景 3: 服务端重启后客户端自动重连"""
    clients = []
    for i in range(client_count):
        c = StableClient(9200 + i, host)
        if c.connect_and_login():
            clients.append(c)
        else:
            c.close()

    if not clients:
        return {"error": "无客户端连接成功"}

    initial_count = len(clients)

    # 验证所有客户端能正常通信
    for c in clients:
        c.send_heartbeat()

    # 重启服务端
    print(f"    重启服务端 ...", end=" ", flush=True)
    t_restart = time.monotonic()
    if not harness.restart():
        print("❌")
        return {"error": "服务端重启失败"}
    print("✅")

    # 等待服务端就绪
    time.sleep(2)

    # 尝试重连
    reconnect_success = 0
    for c in clients:
        if c.reconnect():
            reconnect_success += 1

    # 验证重连后能正常通信
    post_reconnect_ok = 0
    for c in clients:
        if c.logged_in and c.send_heartbeat():
            post_reconnect_ok += 1

    elapsed = time.monotonic() - t_restart

    for c in clients:
        c.close()

    return {
        "initial_clients": initial_count,
        "reconnect_success": reconnect_success,
        "post_reconnect_heartbeat_ok": post_reconnect_ok,
        "reconnect_rate": reconnect_success / initial_count if initial_count > 0 else 0,
        "recovery_time": elapsed,
    }

def test_mixed_workload(host: str, duration: int, client_count: int) -> dict:
    """场景 4: 混合负载（心跳 + 消息发送 + 随机断连）"""
    clients = []
    for i in range(client_count):
        c = StableClient(9300 + i, host)
        if c.connect_and_login():
            clients.append(c)
        else:
            c.close()

    if not clients:
        return {"error": "无客户端连接成功"}

    t0 = time.monotonic()
    total_messages = 0
    total_acked = 0
    total_reconnects = 0
    iteration = 0

    while time.monotonic() - t0 < duration:
        iteration += 1
        alive = []
        for c in clients:
            # 10% 概率断连
            if random.random() < 0.1:
                c.close()
                total_reconnects += 1
                if c.reconnect():
                    alive.append(c)
                continue

            # 50% 概率发消息，50% 发心跳
            if random.random() < 0.5:
                to_uid = random.choice(clients).uid if clients else 9300
                if c.send_message(to_uid, f"mixed_{iteration}"):
                    total_acked += 1
                total_messages += 1
            else:
                c.send_heartbeat()

            alive.append(c)
        clients = alive
        time.sleep(1)

    elapsed = time.monotonic() - t0

    for c in clients:
        c.close()

    return {
        "duration": elapsed,
        "final_clients": len(clients),
        "total_reconnects": total_reconnects,
        "total_messages": total_messages,
        "total_acked": total_acked,
        "message_success_rate": total_acked / total_messages if total_messages > 0 else 0,
    }

# ─── 主流程 ──────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="msrChat 长连接稳定性压测")
    parser.add_argument("--server", default=None, help="ChatServer 路径")
    parser.add_argument("--duration", type=int, default=30, help="每场景持续时间（秒）")
    parser.add_argument("--clients", type=int, default=10, help="客户端数")
    parser.add_argument("--no-server", action="store_true", help="不自动启动服务端")
    parser.add_argument("--host", default="127.0.0.1", help="服务端地址")
    args = parser.parse_args()

    SCRIPT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "server", "ChatServer"))
    BUILD_DIR = os.path.join(SCRIPT_DIR, "build")
    BINARY = args.server or os.path.join(BUILD_DIR, "ChatServer")

    harness = None
    if not args.no_server:
        harness = ServerHarness(BINARY, BUILD_DIR)
        print(f"  启动服务端 ...", end=" ", flush=True)
        if not harness.start():
            print("❌")
            sys.exit(1)
        print("✅")

    try:
        print()
        print("=" * 60)
        print("  msrChat 长连接稳定性压测")
        print("=" * 60)

        # 场景 1: 心跳稳定性
        print(f"\n  场景 1: {args.clients} 连接保持心跳 {args.duration}s")
        print(f"  {'─' * 50}")
        r1 = test_heartbeat_stability(args.host, args.duration, args.clients)
        if "error" not in r1:
            print(f"  初始连接: {r1['initial_clients']}  最终连接: {r1['final_clients']}")
            print(f"  掉线数: {r1['drops']}")
            print(f"  存活率: {r1['survival_rate']*100:.1f}%")
            print(f"  心跳成功率: {r1['heartbeat_success_rate']*100:.1f}%")
        else:
            print(f"  ❌ {r1['error']}")

        # 场景 2: 随机断连重连
        print(f"\n  场景 2: {args.clients} 连接随机断连重连 {args.duration}s (30% 断连率)")
        print(f"  {'─' * 50}")
        r2 = test_reconnect_stability(args.host, args.duration, args.clients, drop_rate=0.3)
        if "error" not in r2:
            print(f"  客户端: {r2['clients']}")
            print(f"  总重连: {r2['total_reconnects']}  成功: {r2['successful_reconnects']}")
            print(f"  重连成功率: {r2['reconnect_success_rate']*100:.1f}%")
            print(f"  最终存活: {r2['final_alive']}")
        else:
            print(f"  ❌ {r2['error']}")

        # 场景 3: 服务端重启恢复
        if harness:
            print(f"\n  场景 3: 服务端重启后 {args.clients} 客户端自动重连")
            print(f"  {'─' * 50}")
            r3 = test_server_restart_recovery(harness, args.host, args.clients)
            if "error" not in r3:
                print(f"  初始连接: {r3['initial_clients']}")
                print(f"  重连成功: {r3['reconnect_success']}")
                print(f"  重连后心跳正常: {r3['post_reconnect_heartbeat_ok']}")
                print(f"  重连率: {r3['reconnect_rate']*100:.1f}%")
                print(f"  恢复耗时: {r3['recovery_time']:.2f}s")
            else:
                print(f"  ❌ {r3['error']}")

        # 场景 4: 混合负载
        print(f"\n  场景 4: {args.clients} 连接混合负载 {args.duration}s (心跳+消息+断连)")
        print(f"  {'─' * 50}")
        r4 = test_mixed_workload(args.host, args.duration, args.clients)
        if "error" not in r4:
            print(f"  最终连接: {r4['final_clients']}")
            print(f"  重连次数: {r4['total_reconnects']}")
            print(f"  消息发送: {r4['total_messages']}  确认: {r4['total_acked']}")
            print(f"  消息成功率: {r4['message_success_rate']*100:.1f}%")
        else:
            print(f"  ❌ {r4['error']}")

        print()
        print("=" * 60)
        print("  压测完成")
        print("=" * 60)

    finally:
        if harness:
            harness.stop()

if __name__ == "__main__":
    main()
