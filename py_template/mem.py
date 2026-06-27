import json
import socket
import struct


class GameMemoryClient:
    def __init__(self, host="127.0.0.1", port=47892, timeout=5.0):
        self.host = host
        self.port = int(port)
        self.timeout = timeout
        self.sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        self.file = self.sock.makefile("rwb", buffering=0)

    def close(self):
        try:
            self.file.close()
        finally:
            self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def request(self, payload):
        line = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
        self.file.write(line)
        response = self.file.readline()
        if not response:
            raise RuntimeError("memory server closed the connection")
        data = json.loads(response.decode("utf-8"))
        if not data.get("ok"):
            raise RuntimeError(data.get("error", "memory server request failed"))
        return data

    def ping(self):
        return self.request({"cmd": "ping"})["data"]

    def module_base(self, module=""):
        return int(self.request({"cmd": "module_base", "module": module})["base"])

    def read_bytes(self, module, offset, size):
        data = self.request({"cmd": "read", "module": module, "offset": int(offset), "size": int(size)})["data"]
        return bytes.fromhex(data)

    def write_bytes(self, module, offset, data):
        self.request({"cmd": "write", "module": module, "offset": int(offset), "data": bytes(data).hex()})

    def read_abs(self, address, size):
        data = self.request({"cmd": "read_abs", "address": int(address), "size": int(size)})["data"]
        return bytes.fromhex(data)

    def write_abs(self, address, data):
        self.request({"cmd": "write_abs", "address": int(address), "data": bytes(data).hex()})

    def _prefix(self, endian):
        if endian in ("big", "be", ">"):
            return ">"
        if endian in ("little", "le", "<"):
            return "<"
        raise ValueError("endian must be 'big' or 'little'")

    def read_int(self, module, offset, endian="big"):
        return struct.unpack(self._prefix(endian) + "i", self.read_bytes(module, offset, 4))[0]

    def write_int(self, module, offset, value, endian="big"):
        self.write_bytes(module, offset, struct.pack(self._prefix(endian) + "i", int(value)))

    def read_uint(self, module, offset, endian="big"):
        return struct.unpack(self._prefix(endian) + "I", self.read_bytes(module, offset, 4))[0]

    def write_uint(self, module, offset, value, endian="big"):
        self.write_bytes(module, offset, struct.pack(self._prefix(endian) + "I", int(value)))

    def read_float(self, module, offset, endian="big"):
        return struct.unpack(self._prefix(endian) + "f", self.read_bytes(module, offset, 4))[0]

    def write_float(self, module, offset, value, endian="big"):
        self.write_bytes(module, offset, struct.pack(self._prefix(endian) + "f", float(value)))

    def read_double(self, module, offset, endian="big"):
        return struct.unpack(self._prefix(endian) + "d", self.read_bytes(module, offset, 8))[0]

    def write_double(self, module, offset, value, endian="big"):
        self.write_bytes(module, offset, struct.pack(self._prefix(endian) + "d", float(value)))

    def read_int_big_endian(self, module, offset):
        return self.read_int(module, offset, "big")

    def write_int_big_endian(self, module, offset, value):
        self.write_int(module, offset, value, "big")

    def read_uint_big_endian(self, module, offset):
        return self.read_uint(module, offset, "big")

    def write_uint_big_endian(self, module, offset, value):
        self.write_uint(module, offset, value, "big")

    def read_float_big_endian(self, module, offset):
        return self.read_float(module, offset, "big")

    def write_float_big_endian(self, module, offset, value):
        self.write_float(module, offset, value, "big")

    def read_double_big_endian(self, module, offset):
        return self.read_double(module, offset, "big")

    def write_double_big_endian(self, module, offset, value):
        self.write_double(module, offset, value, "big")

    def read_int_little_endian(self, module, offset):
        return self.read_int(module, offset, "little")

    def write_int_little_endian(self, module, offset, value):
        self.write_int(module, offset, value, "little")

    def read_uint_little_endian(self, module, offset):
        return self.read_uint(module, offset, "little")

    def write_uint_little_endian(self, module, offset, value):
        self.write_uint(module, offset, value, "little")

    def read_float_little_endian(self, module, offset):
        return self.read_float(module, offset, "little")

    def write_float_little_endian(self, module, offset, value):
        self.write_float(module, offset, value, "little")

    def read_double_little_endian(self, module, offset):
        return self.read_double(module, offset, "little")

    def write_double_little_endian(self, module, offset, value):
        self.write_double(module, offset, value, "little")
