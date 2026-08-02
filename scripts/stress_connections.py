#!/usr/bin/env python3
"""
msrChat 并发连接压测
模拟大量客户端同时连接服务端，测量连接建立性能和资源占用。

用法:
  python3 scripts/stress_connections.py
  python3 scripts/stress_connections.py --count 1000
  python3 scripts/stress_connections.py --server /path/to/ChatServer
"""
import argparse, json, os, signal, socket, struct, subprocess, sys, threading, time

# ─── 协议常量 ────────────────────────────────────────────────
MSG_CHAT_LOGIN = 1005
MSG_HELLO = 1000
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
class ConnClient:
    def __init__(self, uid: int, host: str = "127.0.0.1", port: int = SERVER_PORT):
        self.uid = uid
        self.host = host
        self.port = port
        self.sock = None
        self.connected = False
        self.logged_in = False
        self.connect_time = 0.0

    def connect_and_login(self) -> bool:
        t0 = time.monotonic()
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(10.0)
            self.sock.connect((self.host, self.port))
            self.connected = True
            self.connect_time = time.monotonic() - t0

            body = json.dumps({"uid": self.uid, "token": "dev_token"}).encode()
            self.sock.sendall(build_packet(MSG_CHAT_LOGIN, body))
            msg_id, rsp_body = recv_packet(self.sock)
            if msg_id == MSG_CHAT_LOGIN:
                rsp = json.loads(rsp_body)
                if rsp.get("error") == 0:
                    self.logged_in = True
                    return True
        except Exception as e:
            pass
        return False

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
        self.connected = False
        self.logged_in = False

# ─── 服务端管理 ──────────────────────────────────────────────
class ServerHarness:
    def __init__(self, binary: str, build_dir: str):
        self.binary = binary
        self.build_dir = build_dir
        self.proc = None

    def start(self) -> bool:
        log_path = os.path.join(self.build_dir, "stress_conn_server.log")
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

    def get_rss_mb(self) -> float:
        """获取服务端进程 RSS 内存（MB）"""
        try:
            import psutil
            p = psutil.Process(self.proc.pid)
            return p.memory_info().rss / 1024 / 1024
        except ImportError:
            # psutil 不可用，读 /proc
            try:
                with open(f"/proc/{self.proc.pid}/status") as f:
                    for line in f:
                        if line.startswith("VmRSS:"):
                            return int(line.split()[1]) / 1024
            except:
                pass
        return -1

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

def stress_concurrent_connect(count: int, host: str = "127.0.0.1") -> dict:
    """并发连接压测：count 个客户端同时连接并登录"""
    clients = []
    connect_times = []
    success = 0
    fail = 0

    results = {"count": count, "success": 0, "fail": 0, "times": []}

    # 并发连接
    t0 = time.monotonic()
    threads = []
    lock = threading.Lock()

    def worker(uid):
        nonlocal success, fail
        c = ConnClient(uid, host)
        ok = c.connect_and_login()
        with lock:
            if ok:
                success += 1
                connect_times.append(c.connect_time)
            else:
                fail += 1
            clients.append(c)

    for i in range(count):
        t = threading.Thread(target=worker, args=(10000 + i,))
        threads.append(t)

    # 启动所有线程
    for t in threads:
        t.start()

    # 等待完成
    for t in threads:
        t.join(timeout=30)

    elapsed = time.monotonic() - t0

    results["success"] = success
    results["fail"] = fail
    results["elapsed"] = elapsed
    results["conn_per_sec"] = success / elapsed if elapsed > 0 else 0
    results["times"] = connect_times

    # 清理
    for c in clients:
        c.close()

    return results

def stress_ramp_up(max_count: int, step: int = 100, host: str = "127.0.0.1") -> list:
    """线性增长压测：从 step 到 max_count，每步增加 step 个连接"""
    results = []
    active_clients = []

    for target in range(step, max_count + 1, step):
        new_count = step
        new_clients = []

        t0 = time.monotonic()
        success = 0
        fail = 0

        for i in range(new_count):
            uid = 20000 + target * 100 + i
            c = ConnClient(uid, host)
            ok = c.connect_and_login()
            if ok:
                success += 1
                new_clients.append(c)
            else:
                fail += 1
                c.close()

        elapsed = time.monotonic() - t0
        active_clients.extend(new_clients)

        results.append({
            "total_connections": target,
            "new_success": success,
            "new_fail": fail,
            "cumulative_active": len(active_clients),
            "elapsed": elapsed,
        })

    # 清理
    for c in active_clients:
        c.close()

    return results

# ─── 主流程 ──────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="msrChat 并发连接压测")
    parser.add_argument("--server", default=None, help="ChatServer 路径")
    parser.add_argument("--count", type=int, default=500, help="并发连接数")
    parser.add_argument("--ramp", action="store_true", help="运行线性增长压测")
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
        print("  msrChat 并发连接压测")
        print("=" * 60)

        # 场景 1: 并发连接
        print(f"\n  场景 1: {args.count} 并发连接 + 登录")
        print(f"  {'─' * 50}")
        r = stress_concurrent_connect(args.count, args.host)
        pcts = percentiles(r["times"])
        print(f"  成功: {r['success']}/{r['count']}")
        print(f"  失败: {r['fail']}")
        print(f"  耗时: {r['elapsed']:.2f}s")
        print(f"  连接速率: {r['conn_per_sec']:.0f} conn/s")
        if r["times"]:
            print(f"  连接耗时 P50: {pcts[50]*1000:.1f}ms")
            print(f"  连接耗时 P95: {pcts[95]*1000:.1f}ms")
            print(f"  连接耗时 P99: {pcts[99]*1000:.1f}ms")
        if harness:
            rss = harness.get_rss_mb()
            if rss > 0:
                print(f"  服务端 RSS: {rss:.1f}MB")

        # 场景 2: 线性增长
        if args.ramp:
            print(f"\n  场景 2: 线性增长到 {args.count} 连接")
            print(f"  {'─' * 50}")
            ramp_results = stress_ramp_up(args.count, step=max(50, args.count // 10), host=args.host)
            print(f"  {'总连接':>8} {'新增成功':>8} {'新增失败':>8} {'累计活跃':>8} {'耗时':>8}")
            for r in ramp_results:
                print(f"  {r['total_connections']:>8} {r['new_success']:>8} {r['new_fail']:>8} "
                      f"{r['cumulative_active']:>8} {r['elapsed']:>7.2f}s")

        print()
        print("=" * 60)
        print("  压测完成")
        print("=" * 60)

    finally:
        if harness:
            harness.stop()

if __name__ == "__main__":
    main()
