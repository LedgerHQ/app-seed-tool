"""Minimal GDB Remote Serial Protocol (RSP) client.

Talks directly to the gdbstub speculos/QEMU exposes on the debug port
(``-d``, port 1234), without depending on a real ``gdb`` binary. Stdlib only.

Only implements what verify_bip39_cancel_clears_buffer.py needs: resume
execution, insert/remove a software breakpoint, single-step, read general
registers, read memory, and redeliver a signal (used to pass SIGILL through
-- see the README in this directory for why that's required).
"""
import socket


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
