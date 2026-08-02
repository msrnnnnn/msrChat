#!/usr/bin/env python3
"""
msrChat ASAN 自动化检测脚本
启动服务端(Debug+ASAN) → 模拟客户端收发 → 优雅关闭 → 检查泄漏报告

用法:
  python3 scripts/asan_benchmark.py
  python3 scripts/asan_benchmark.py --build-only
  python3 scripts/asan_benchmark.py --server /path/to/ChatServer
"""
import argparse, json, os, signal, socket, struct, subprocess, sys, time

# ─── 协议常量 ────────────────────────────────────────────────
MSG_HELLO       = 1000
MSG_CHAT_LOGIN  = 1005
MSG_CHAT_TEXT   = 1006
MSG_CHAT_ACK    = 1007
HEAD_TOTAL_LEN  = 6
SERVER_PORT     = 8080

# ─── TLV 协议辅助 ──────────────────────────────────────────
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

def recv_packet(sock: socket.socket):
    header = recv_n(sock, HEAD_TOTAL_LEN)
    if header is None:
        return None, None
    msg_id, body_len = struct.unpack('!HI', header)
    body = recv_n(sock, body_len)
    if body is None:
        return None, None
    return msg_id, body

# ─── Protobuf 手动构造 (ChatTextMsg) ──────────────────────
def make_chat_text_msg(from_uid: int, to_uid: int, content: str,
                       client_msg_id: str) -> bytes:
    """手动编码 ChatTextMsg protobuf，避免依赖 protobuf Python 包"""
    buf = bytearray()
    def varint(v):
        while v > 0x7f:
            buf.append((v & 0x7f) | 0x80)
            v >>= 7
        buf.append(v)
    def field(tag, typ, val):
        varint((tag << 3) | typ)
        buf.extend(val)
    # from_uid (int32, field=1)
    b = bytearray()
    v = from_uid
    while v > 0x7f:
        b.append((v & 0x7f) | 0x80); v >>= 7
    b.append(v)
    field(1, 0, bytes(b))
    # to_uid (int32, field=2)
    b = bytearray()
    v = to_uid
    while v > 0x7f:
        b.append((v & 0x7f) | 0x80); v >>= 7
    b.append(v)
    field(2, 0, bytes(b))
    # content (string, field=3)
    c = content.encode('utf-8')
    v = len(c)
    b = bytearray()
    while v > 0x7f:
        b.append((v & 0x7f) | 0x80); v >>= 7
    b.append(v)
    field(3, 2, bytes(b) + c)
    # client_msg_id (string, field=4)
    cid = client_msg_id.encode('utf-8')
    v = len(cid)
    b = bytearray()
    while v > 0x7f:
        b.append((v & 0x7f) | 0x80); v >>= 7
    b.append(v)
    field(4, 2, bytes(b) + cid)
    return bytes(buf)

# ─── 客户端模拟 ────────────────────────────────────────────
class BenchmarkClient:
    def __init__(self, uid: int, token: str = "dev_token",
                 host: str = "127.0.0.1", port: int = SERVER_PORT):
        self.uid = uid
        self.token = token
        self.host = host
        self.port = port
        self.sock = None
        self.logged_in = False
        self.sent = 0
        self.received = 0
        self.failed = False

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((self.host, self.port))
        return True

    def login(self):
        body = json.dumps({"uid": self.uid, "token": self.token}).encode()
        self.sock.sendall(build_packet(MSG_CHAT_LOGIN, body))
        msg_id, body = recv_packet(self.sock)
        if msg_id == MSG_CHAT_LOGIN:
            rsp = json.loads(body)
            if rsp.get("error") == 0:
                self.logged_in = True
                return True
        return False

    def send_hello(self):
        self.sock.sendall(build_packet(MSG_HELLO, b'{}'))
        self.sent += 1
        msg_id, _ = recv_packet(self.sock)
        if msg_id == MSG_HELLO:
            self.received += 1
            return True
        self.failed = True
        return False

    def send_chat_text(self, to_uid: int, content: str):
        body = make_chat_text_msg(self.uid, to_uid, content,
                                  f"bench_{self.uid}_{self.sent}")
        self.sock.sendall(build_packet(MSG_CHAT_TEXT, body))
        self.sent += 1
        msg_id, _ = recv_packet(self.sock)
        if msg_id == MSG_CHAT_ACK:
            self.received += 1
            return True
        self.failed = True
        return False

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.logged_in = False

# ─── 服务端管理 ────────────────────────────────────────────
class ServerHarness:
    def __init__(self, binary: str, build_dir: str, script_dir: str):
        self.binary = binary
        self.build_dir = build_dir
        self.script_dir = script_dir
        self.proc = None
        self.log_path = os.path.join(build_dir, "asan_benchmark.log")

    def build(self):
        print("  [构建] 编译 Debug+ASAN ...", end=" ", flush=True)
        env = os.environ.copy()
        env["BUILD_TYPE"] = "Debug"
        result = subprocess.run(
            [os.path.join(self.script_dir, "build.sh")],
            cwd=self.script_dir, env=env,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if result.returncode != 0:
            print("❌ 编译失败")
            return False
        print("✅")
        return True

    def start(self):
        print("  [启动] 启动服务端 ...", end=" ", flush=True)
        log = open(self.log_path, "w")
        self.proc = subprocess.Popen(
            [self.binary],
            cwd=self.build_dir,
            stdout=log, stderr=log)
        for _ in range(20):
            time.sleep(0.3)
            try:
                s = socket.socket()
                s.settimeout(0.5)
                s.connect(("127.0.0.1", SERVER_PORT))
                s.close()
                print(f"✅ PID={self.proc.pid}")
                return True
            except (ConnectionRefusedError, OSError):
                continue
        print("❌ 超时")
        return False

    def graceful_stop(self):
        if not self.proc:
            return
        print("  [关闭] 优雅关闭 ...", end=" ", flush=True)
        self.proc.send_signal(signal.SIGINT)
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
            print("(强制终止)", end=" ")
        print("✅")

    def check_asan(self) -> str:
        if not os.path.exists(self.log_path):
            return "⚠️  日志文件不存在"
        with open(self.log_path) as f:
            content = f.read()
        if not content.strip():
            return "⚠️  日志为空"
        if "0 memory leaks detected" in content:
            return "✅ 0 memory leaks detected"
        if "ERROR: AddressSanitizer" in content:
            lines = [l for l in content.split('\n')
                     if 'ERROR: AddressSanitizer' in l]
            return f"❌ 检测到 {len(lines)} 个泄漏:\n" + "\n".join(lines)
        return "⚠️  未检测到 ASAN 输出"

# ─── 测试场景 ──────────────────────────────────────────────
TOTAL = 5
PASS = 0
FAIL = 0

def check(step: int, desc: str, ok: bool):
    global PASS, FAIL
    if ok:
        PASS += 1
        print(f"  [{step}/{TOTAL}] {desc}  ✅")
    else:
        FAIL += 1
        print(f"  [{step}/{TOTAL}] {desc}  ❌")

def test_normal() -> bool:
    """单客户端登录 + 5 次心跳"""
    c = BenchmarkClient(1001)
    try:
        c.connect() and c.login()
        for _ in range(5):
            c.send_hello()
        return not c.failed
    finally:
        c.close()

def test_multi_client() -> bool:
    """5 客户端并发收发"""
    clients = [BenchmarkClient(2000 + i) for i in range(5)]
    try:
        for c in clients:
            c.connect() and c.login()
        for c in clients:
            for _ in range(3):
                c.send_hello()
        return all(not c.failed for c in clients)
    finally:
        for c in clients:
            c.close()

def test_reconnect(server: ServerHarness) -> bool:
    """断连→重连→恢复"""
    c = BenchmarkClient(3001)
    try:
        c.connect() and c.login()
        c.send_hello()
        server.graceful_stop()
        server.start()
        c.connect() and c.login()
        c.send_hello()
        return not c.failed
    finally:
        c.close()

def test_chat_text() -> bool:
    """MSG_CHAT_TEXT 消息（含 SQLite 写入）"""
    c1 = BenchmarkClient(4001)
    c2 = BenchmarkClient(4002)
    try:
        c1.connect() and c1.login()
        c2.connect() and c2.login()
        for i in range(3):
            c1.send_chat_text(4002, f"hello from 4001 #{i}")
            c2.send_chat_text(4001, f"hello from 4002 #{i}")
        return not c1.failed and not c2.failed
    finally:
        c1.close()
        c2.close()

def test_aggressive_close() -> bool:
    """10 次快速连接/断开"""
    ok = True
    for i in range(10):
        c = BenchmarkClient(5000 + i)
        try:
            c.connect()
            if i % 2 == 0:
                c.login()
                c.send_hello()
        except Exception:
            ok = False
        finally:
            c.close()
    return ok

# ─── 主流程 ──────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="msrChat ASAN 自动化检测")
    parser.add_argument("--server", default=None,
                        help="ChatServer 可执行文件路径")
    parser.add_argument("--build-only", action="store_true",
                        help="只编译不运行")
    parser.add_argument("--stress-all", action="store_true",
                        help="运行全部压测（并发连接/消息吞吐/长连接稳定性）")
    parser.add_argument("--stress-connections", action="store_true",
                        help="运行并发连接压测")
    parser.add_argument("--stress-throughput", action="store_true",
                        help="运行消息吞吐压测")
    parser.add_argument("--stress-stability", action="store_true",
                        help="运行长连接稳定性压测")
    parser.add_argument("--no-server", action="store_true",
                        help="不自动启动服务端（压测模式用）")
    args = parser.parse_args()

    SCRIPT_DIR = os.path.abspath(
        os.path.join(os.path.dirname(__file__), "..", "server", "ChatServer"))
    BUILD_DIR = os.path.join(SCRIPT_DIR, "build")
    BINARY = args.server or os.path.join(BUILD_DIR, "ChatServer")

    print("=" * 50)
    print("  msrChat ASAN 自动化检测")
    print("=" * 50)
    print()

    harness = ServerHarness(BINARY, BUILD_DIR, SCRIPT_DIR)
    if not harness.build():
        sys.exit(1)
    if args.build_only:
        print("  编译完成，跳过运行")
        return

    if not harness.start():
        sys.exit(1)

    # ─── 压测模式 ──────────────────────────────────────
    if args.stress_all or args.stress_connections or args.stress_throughput or args.stress_stability:
        import subprocess as sp
        script_dir = os.path.dirname(os.path.abspath(__file__))
        server_arg = ["--server", BINARY] if args.server else ["--no-server"]
        host_arg = ["--host", "127.0.0.1"]

        # 先启动服务端
        harness = ServerHarness(BINARY, BUILD_DIR, os.path.dirname(BINARY))
        if not args.no_server:
            if not harness.build():
                sys.exit(1)
            if not harness.start():
                sys.exit(1)

        try:
            scripts = []
            if args.stress_all or args.stress_connections:
                scripts.append(("stress_connections.py", ["--count", "500", "--ramp"]))
            if args.stress_all or args.stress_throughput:
                scripts.append(("stress_throughput.py", ["--clients", "10", "--messages", "50"]))
            if args.stress_all or args.stress_stability:
                scripts.append(("stress_stability.py", ["--duration", "30", "--clients", "10"]))

            for script_name, extra_args in scripts:
                script_path = os.path.join(script_dir, script_name)
                cmd = [sys.executable, script_path, "--no-server"] + extra_args
                print(f"\n{'='*60}")
                print(f"  运行 {script_name}")
                print(f"{'='*60}")
                sp.run(cmd)
        finally:
            harness.graceful_stop()

        return

    # ─── ASAN 检测模式 ─────────────────────────────────
    print("=" * 50)
    print("  msrChat ASAN 自动化检测")
    print("=" * 50)
    print()

    harness = ServerHarness(BINARY, BUILD_DIR, SCRIPT_DIR)
    if not harness.build():
        sys.exit(1)
    if args.build_only:
        print("  编译完成，跳过运行")
        return

    if not harness.start():
        sys.exit(1)

    print()
    print("─" * 50)
    print("  测试场景")
    print("─" * 50)

    check(1, "单客户端 5 次心跳", test_normal())
    check(2, "5 客户端并发 15 次心跳", test_multi_client())
    check(3, "断连重连 3 次心跳", test_reconnect(harness))
    check(4, "双客户端 6 条聊天消息", test_chat_text())
    check(5, "10 次快速连/断", test_aggressive_close())

    print()
    harness.graceful_stop()
    print()

    result = harness.check_asan()
    print("=" * 50)
    print(f"  ASAN 检测结果: {result}")
    print(f"  场景: {PASS}/{TOTAL} 通过")
    print("=" * 50)

    sys.exit(0 if FAIL == 0 else 1)

if __name__ == "__main__":
    main()
