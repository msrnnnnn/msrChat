#!/usr/bin/env python3
"""
msrChat 消息吞吐压测
测量消息处理性能、ACK 延迟、RateLimiter 限流正确性。

用法:
  python3 scripts/stress_throughput.py
  python3 scripts/stress_throughput.py --clients 100 --messages 50
  python3 scripts/stress_throughput.py --server /path/to/ChatServer
"""
import argparse, json, os, signal, socket, struct, subprocess, sys, threading, time, uuid

# ─── 协议常量 ────────────────────────────────────────────────
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

# ─── Protobuf 手动构造 ──────────────────────────────────────
def varint_encode(v: int) -> bytes:
    buf = bytearray()
    while v > 0x7f:
        buf.append((v & 0x7f) | 0x80)
        v >>= 7
    buf.append(v)
    return bytes(buf)

def make_chat_text_msg(from_uid: int, to_uid: int, content: str, client_msg_id: str) -> bytes:
    buf = bytearray()
    def add_field(tag: int, wire_type: int, val: bytes):
        buf.extend(varint_encode((tag << 3) | wire_type))
        buf.extend(val)

    add_field(1, 0, varint_encode(from_uid))
    add_field(2, 0, varint_encode(to_uid))
    c = content.encode('utf-8')
    add_field(3, 2, varint_encode(len(c)) + c)
    cid = client_msg_id.encode('utf-8')
    add_field(4, 2, varint_encode(len(cid)) + cid)
    return bytes(buf)

# ─── 客户端 ──────────────────────────────────────────────────
class ThroughputClient:
    def __init__(self, uid: int, host: str = "127.0.0.1", port: int = SERVER_PORT):
        self.uid = uid
        self.host = host
        self.port = port
        self.sock = None
        self.sent = 0
        self.acked = 0
        self.rate_limited = 0
        self.errors = 0
        self.ack_latencies = []

    def connect_and_login(self) -> bool:
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(10.0)
            self.sock.connect((self.host, self.port))
            body = json.dumps({"uid": self.uid, "token": "dev_token"}).encode()
            self.sock.sendall(build_packet(MSG_CHAT_LOGIN, body))
            msg_id, rsp_body = recv_packet(self.sock)
            if msg_id == MSG_CHAT_LOGIN:
                rsp = json.loads(rsp_body)
                return rsp.get("error") == 0
        except:
            pass
        return False

    def send_message(self, to_uid: int, content: str) -> bool:
        msg_id = str(uuid.uuid4())
        body = make_chat_text_msg(self.uid, to_uid, content, msg_id)
        t0 = time.monotonic()
        try:
            self.sock.sendall(build_packet(MSG_CHAT_TEXT, body))
            self.sent += 1
            ack_id, ack_body = recv_packet(self.sock, timeout=10.0)
            latency = time.monotonic() - t0
            if ack_id == MSG_CHAT_ACK:
                ack = json.loads(ack_body)
                self.ack_latencies.append(latency)
                if ack.get("error") == 0:
                    self.acked += 1
                    return True
                elif ack.get("error") == 1015:  # ERR_RATE_LIMITED
                    self.rate_limited += 1
                    return False
                else:
                    self.errors += 1
                    return False
        except:
            pass
        self.errors += 1
        return False

    def send_burst(self, to_uid: int, count: int, content_prefix: str = "msg"):
        """发送一批消息，不等待 ACK（用于吞吐测试）"""
        for i in range(count):
            msg_id = str(uuid.uuid4())
            body = make_chat_text_msg(self.uid, to_uid, f"{content_prefix}_{i}", msg_id)
            try:
                self.sock.sendall(build_packet(MSG_CHAT_TEXT, body))
                self.sent += 1
            except:
                self.errors += 1

    def recv_all_acks(self, expected: int, timeout: float = 30.0):
        """接收所有待处理的 ACK"""
        deadline = time.monotonic() + timeout
        while self.acked + self.rate_limited + self.errors < expected:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            try:
                ack_id, ack_body = recv_packet(self.sock, timeout=min(remaining, 1.0))
                if ack_id == MSG_CHAT_ACK:
                    ack = json.loads(ack_body)
                    if ack.get("error") == 0:
                        self.acked += 1
                    elif ack.get("error") == 1015:
                        self.rate_limited += 1
                    else:
                        self.errors += 1
            except socket.timeout:
                continue
            except:
                break

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass

# ─── 服务端管理 ──────────────────────────────────────────────
class ServerHarness:
    def __init__(self, binary: str, build_dir: str):
        self.binary = binary
        self.build_dir = build_dir
        self.proc = None

    def start(self) -> bool:
        log_path = os.path.join(self.build_dir, "stress_throughput_server.log")
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

# ─── 压测场景 ────────────────────────────────────────────────
def percentiles(values: list, pcts: list = [50, 95, 99]) -> dict:
    if not values:
        return {p: 0 for p in pcts}
    values = sorted(values)
    result = {}
    for p in pcts:
        idx = int(len(values) * p / 100)
        idx = min(idx, len(values) - 1)
        result[p] = values[idx]
    return result

def test_single_connection_throughput(host: str, msg_count: int = 100) -> dict:
    """场景 1: 单连接高频发送，测量 ACK 延迟"""
    c = ThroughputClient(6001, host)
    if not c.connect_and_login():
        return {"error": "连接失败"}

    latencies = []
    t0 = time.monotonic()
    for i in range(msg_count):
        msg_id = str(uuid.uuid4())
        body = make_chat_text_msg(6001, 6002, f"throughput_{i}", msg_id)
        send_t = time.monotonic()
        c.sock.sendall(build_packet(MSG_CHAT_TEXT, body))
        c.sent += 1
        try:
            ack_id, ack_body = recv_packet(c.sock, timeout=10.0)
            lat = time.monotonic() - send_t
            if ack_id == MSG_CHAT_ACK:
                latencies.append(lat)
                ack = json.loads(ack_body)
                if ack.get("error") == 0:
                    c.acked += 1
                else:
                    c.errors += 1
        except:
            c.errors += 1
    elapsed = time.monotonic() - t0
    c.close()

    pcts = percentiles(latencies)
    return {
        "sent": c.sent,
        "acked": c.acked,
        "errors": c.errors,
        "elapsed": elapsed,
        "msg_per_sec": c.acked / elapsed if elapsed > 0 else 0,
        "latency_p50_ms": pcts[50] * 1000,
        "latency_p95_ms": pcts[95] * 1000,
        "latency_p99_ms": pcts[99] * 1000,
    }

def test_multi_client_throughput(host: str, client_count: int, msg_per_client: int) -> dict:
    """场景 2: 多客户端并发发送"""
    clients = []
    for i in range(client_count):
        c = ThroughputClient(7000 + i, host)
        if c.connect_and_login():
            clients.append(c)
        else:
            c.close()

    if not clients:
        return {"error": "无客户端连接成功"}

    total_sent = 0
    total_acked = 0
    total_errors = 0
    all_latencies = []

    t0 = time.monotonic()

    # 并发发送
    threads = []
    lock = threading.Lock()

    def worker(client: ThroughputClient):
        for i in range(msg_per_client):
            msg_id = str(uuid.uuid4())
            body = make_chat_text_msg(client.uid, 7000, f"multi_{client.uid}_{i}", msg_id)
            send_t = time.monotonic()
            try:
                client.sock.sendall(build_packet(MSG_CHAT_TEXT, body))
                client.sent += 1
                ack_id, ack_body = recv_packet(client.sock, timeout=10.0)
                lat = time.monotonic() - send_t
                if ack_id == MSG_CHAT_ACK:
                    with lock:
                        all_latencies.append(lat)
                    ack = json.loads(ack_body)
                    if ack.get("error") == 0:
                        client.acked += 1
                    elif ack.get("error") == 1015:
                        client.rate_limited += 1
                    else:
                        client.errors += 1
            except:
                client.errors += 1

    for c in clients:
        t = threading.Thread(target=worker, args=(c,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join(timeout=60)

    elapsed = time.monotonic() - t0

    for c in clients:
        total_sent += c.sent
        total_acked += c.acked
        total_errors += c.errors + c.rate_limited

    for c in clients:
        c.close()

    pcts = percentiles(all_latencies)
    return {
        "clients": len(clients),
        "total_sent": total_sent,
        "total_acked": total_acked,
        "total_errors": total_errors,
        "elapsed": elapsed,
        "msg_per_sec": total_acked / elapsed if elapsed > 0 else 0,
        "latency_p50_ms": pcts[50] * 1000,
        "latency_p95_ms": pcts[95] * 1000,
        "latency_p99_ms": pcts[99] * 1000,
    }

def test_rate_limiter(host: str) -> dict:
    """场景 3: 触发 RateLimiter 限流"""
    c = ThroughputClient(8001, host)
    if not c.connect_and_login():
        return {"error": "连接失败"}

    t0 = time.monotonic()
    # 高频发送，不等待 ACK
    burst_count = 200
    for i in range(burst_count):
        msg_id = str(uuid.uuid4())
        body = make_chat_text_msg(8001, 8002, f"rate_{i}", msg_id)
        try:
            c.sock.sendall(build_packet(MSG_CHAT_TEXT, body))
            c.sent += 1
        except:
            c.errors += 1

    # 接收所有 ACK
    c.recv_all_acks(burst_count, timeout=30.0)
    elapsed = time.monotonic() - t0
    c.close()

    return {
        "sent": c.sent,
        "acked": c.acked,
        "rate_limited": c.rate_limited,
        "errors": c.errors,
        "elapsed": elapsed,
        "limiting_active": c.rate_limited > 0,
    }

# ─── 主流程 ──────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="msrChat 消息吞吐压测")
    parser.add_argument("--server", default=None, help="ChatServer 路径")
    parser.add_argument("--clients", type=int, default=10, help="并发客户端数")
    parser.add_argument("--messages", type=int, default=50, help="每客户端消息数")
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
        print("  msrChat 消息吞吐压测")
        print("=" * 60)

        # 场景 1: 单连接吞吐
        print(f"\n  场景 1: 单连接 100 条消息，测量 ACK 延迟")
        print(f"  {'─' * 50}")
        r1 = test_single_connection_throughput(args.host, 100)
        if "error" not in r1:
            print(f"  发送: {r1['sent']}  确认: {r1['acked']}  错误: {r1['errors']}")
            print(f"  吞吐量: {r1['msg_per_sec']:.0f} msg/s")
            print(f"  ACK 延迟 P50: {r1['latency_p50_ms']:.2f}ms")
            print(f"  ACK 延迟 P95: {r1['latency_p95_ms']:.2f}ms")
            print(f"  ACK 延迟 P99: {r1['latency_p99_ms']:.2f}ms")
        else:
            print(f"  ❌ {r1['error']}")

        # 场景 2: 多客户端并发吞吐
        print(f"\n  场景 2: {args.clients} 客户端各发 {args.messages} 条消息")
        print(f"  {'─' * 50}")
        r2 = test_multi_client_throughput(args.host, args.clients, args.messages)
        if "error" not in r2:
            print(f"  客户端: {r2['clients']}")
            print(f"  总发送: {r2['total_sent']}  总确认: {r2['total_acked']}  总错误: {r2['total_errors']}")
            print(f"  总吞吐量: {r2['msg_per_sec']:.0f} msg/s")
            print(f"  ACK 延迟 P50: {r2['latency_p50_ms']:.2f}ms")
            print(f"  ACK 延迟 P95: {r2['latency_p95_ms']:.2f}ms")
            print(f"  ACK 延迟 P99: {r2['latency_p99_ms']:.2f}ms")
        else:
            print(f"  ❌ {r2['error']}")

        # 场景 3: RateLimiter 限流测试
        print(f"\n  场景 3: RateLimiter 限流测试 (200 条突发)")
        print(f"  {'─' * 50}")
        r3 = test_rate_limiter(args.host)
        if "error" not in r3:
            print(f"  发送: {r3['sent']}  确认: {r3['acked']}  限流: {r3['rate_limited']}  错误: {r3['errors']}")
            if r3["limiting_active"]:
                print(f"  ✅ RateLimiter 限流生效，拦截了 {r3['rate_limited']} 条消息")
            else:
                print(f"  ⚠️  未触发限流（可能限流阈值较高或测试量不足）")
        else:
            print(f"  ❌ {r3['error']}")

        print()
        print("=" * 60)
        print("  压测完成")
        print("=" * 60)

    finally:
        if harness:
            harness.stop()

if __name__ == "__main__":
    main()
