"""Minimal GDB Remote Serial Protocol (RSP) client.

Talks directly to the gdbstub speculos/QEMU exposes on the debug port
(``-d``, port 1234), without depending on a real ``gdb`` binary. Stdlib only.

Only implements what the verify_*.py checks in this directory need: resume
execution, insert/remove a software breakpoint, single-step, read general
registers, read memory, and redeliver a signal (used to pass SIGILL through
-- see the README in this directory for why that's required).

Also provides wait_for_gdb(), the startup-race guard every verify_*.py
script needs before opening its RSP connection.
"""
import socket
import time


def wait_for_gdb(port=1234, host="localhost", timeout=15):
    """Wait for Speculos's GDB stub to come up before connecting to it.
    Returns True once it looks ready, False if `timeout` elapses first
    (callers report that as their own failure).

    A fixed sleep after `docker run` is not enough: how long the stub takes
    to start listening varies with the host and its load, and connecting too
    early fails with `ConnectionResetError` on the first RSP command rather
    than at connect() time.

    Deliberately probes with a bare TCP connect and nothing else. It is
    tempting to make this stricter by sending a real RSP packet ("?") and
    requiring a well-formed reply, since a plain connect() can succeed
    before the stub is truly ready -- with Docker's userland proxy the host
    port is bound the instant the container starts, and one host measured
    TCP accepted at +0.20s but the stub only answering at +0.70s, with
    ConnectionResetError in between. Do not do that: QEMU's gdbstub serves a
    single GDB session, so a throwaway connection that actually speaks RSP
    and then disconnects reads as a client attaching and detaching, and the
    real connection that follows then hangs on its first command. That was
    tried here and broke the run; the trailing sleep below is the
    deliberate, working alternative.
    """
    start = time.time()
    while time.time() - start < timeout:
        try:
            s = socket.create_connection((host, port), timeout=1)
            s.close()
            time.sleep(1)  # brief buffer for GDB stub handshake readiness
            return True
        except (ConnectionRefusedError, OSError):
            time.sleep(0.5)
    return False


class RSP:
    def __init__(self, host="localhost", port=1234, timeout=40):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.s.settimeout(timeout)
        self.s.connect((host, port))

    def _checksum(self, data: bytes) -> bytes:
        return ("%02x" % (sum(data) % 256)).encode()

    def _send_packet(self, data: bytes):
        self.s.sendall(b"$" + data + b"#" + self._checksum(data))

    def _read_byte(self):
        b = self.s.recv(1)
        if not b:
            raise ConnectionError("socket closed")
        return b

    def _read_ack(self):
        b = self._read_byte()
        while b not in (b"+", b"-"):
            b = self._read_byte()
        return b

    def _read_packet(self):
        b = self._read_byte()
        while b != b"$":
            b = self._read_byte()
        data = ""
        while True:
            b = self._read_byte()
            if b == b"#":
                break
            data += b.decode()
        self.s.recv(2)  # 2 hex checksum bytes, not verified
        self.s.sendall(b"+")
        return data

    def send_command(self, cmd: str) -> str:
        self._send_packet(cmd.encode())
        if self._read_ack() != b"+":
            raise RuntimeError(f"command {cmd!r} was NACKed")
        return self._read_packet()

    def cont(self):
        """Resume execution. Does not wait for a stop-reply -- the stub
        won't send one until the target actually stops again."""
        self._send_packet(b"c")

    def cont_with_signal(self, sig: int):
        """Resume, redelivering signal `sig` to the guest instead of
        swallowing it. Used to pass SIGILL through."""
        self._send_packet(f"C{sig:02x}".encode())

    def wait_stop(self) -> str:
        """Block until the next stop-reply packet and return it raw
        (e.g. 'T05...' or 'S05')."""
        return self._read_packet()

    @staticmethod
    def stop_signal(reply: str) -> int:
        return int(reply[1:3], 16)

    def insert_bp(self, addr: int, kind: int = 2):
        if self.send_command(f"Z0,{addr:x},{kind}") != "OK":
            raise RuntimeError(f"insert_bp(0x{addr:x}) failed")

    def remove_bp(self, addr: int, kind: int = 2):
        if self.send_command(f"z0,{addr:x},{kind}") != "OK":
            raise RuntimeError(f"remove_bp(0x{addr:x}) failed")

    def step(self):
        """Single-step one instruction; blocks for the resulting stop-reply."""
        self._send_packet(b"s")
        if self._read_ack() != b"+":
            raise RuntimeError("step was NACKed")
        return self._read_packet()

    def read_regs(self) -> list:
        """r0..r15 as unsigned 32-bit ints (r9=data base, r13=SP, r14=LR, r15=PC)."""
        raw = self.send_command("g")
        words = [raw[i:i + 8] for i in range(0, len(raw), 8)]
        return [int.from_bytes(bytes.fromhex(w), "little") for w in words]

    def read_mem(self, addr: int, length: int) -> bytes:
        reply = self.send_command(f"m{addr:x},{length:x}")
        if reply.startswith("E"):
            raise RuntimeError(f"read_mem(0x{addr:x}, {length}) failed: {reply}")
        return bytes.fromhex(reply)
