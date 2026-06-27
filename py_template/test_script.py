import os
import time
from datetime import datetime

from mem import GameMemoryClient


def main():
    host = os.environ.get("SKATE_MEM_HOST", "127.0.0.1")
    port = int(os.environ.get("SKATE_MEM_PORT", "47892"))
    game_dir = os.environ.get("SKATE_MOD_GAME_DIR")
    py_dir = os.environ.get("SKATE_MOD_PY_DIR", os.path.dirname(os.path.abspath(__file__)))
    output_dir = game_dir if game_dir else py_dir
    output_path = os.path.join(output_dir, "python_test_success.txt")

    last_error = None
    result = None
    deadline = time.time() + 8.0
    while time.time() < deadline:
        try:
            with GameMemoryClient(host, port, timeout=1.0) as mem:
                result = mem.ping()
            break
        except Exception as exc:
            last_error = exc
            time.sleep(0.25)

    with open(output_path, "w", encoding="utf-8") as f:
        f.write("timestamp=" + datetime.now().isoformat() + "\n")
        if result is not None:
            f.write("ping=" + str(result) + "\n")
        else:
            f.write("error=" + repr(last_error) + "\n")


if __name__ == "__main__":
    main()
