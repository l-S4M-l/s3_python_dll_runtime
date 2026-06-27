# python_runner.dll

`python_runner.dll` is a Windows x64 C++ mod DLL intended to be loaded by an existing `XInput1_4.dll` proxy/modloader from `GameFolder\Mods`.

When loaded, it starts a small worker thread, creates `GameFolder\Mods\py`, starts a localhost-only TCP memory server, and launches user Python scripts from that folder without opening a terminal window.

## Requirements

- Windows x64
- CMake
- Visual Studio 2022 Build Tools with Desktop development with C++
- Python installed on Windows

## Build

Run:

```bat
build.bat
```

The DLL is written to:

```text
dist\python_runner.dll
```

The default Python helper files are copied to:

```text
dist\py
```

## Install

Copy:

```text
dist\python_runner.dll
```

to:

```text
GameFolder\Mods\python_runner.dll
```

Then launch the game. The DLL will create `GameFolder\Mods\py` and default files if they are missing. You can also manually copy `dist\py` to `GameFolder\Mods\py`.

## First Run

After launching the game, confirm:

- `GameFolder\Mods\py` exists
- `GameFolder\Mods\py\mem.py` exists
- `GameFolder\python_runner.log` exists
- `GameFolder\python_test_success.txt` exists if `test_script.py` ran

## Config

The DLL creates this file if missing:

```text
GameFolder\Mods\python_runner.ini
```

Default contents:

```ini
port=47892
auto_launch_python=true
```

Supported options:

- `port`: localhost TCP port for the memory server
- `auto_launch_python`: `true` or `false`

The TCP server only binds to `127.0.0.1`.

## Python Scripts

Auto-run rules:

- Runs `.py` files directly inside `GameFolder\Mods\py`
- Does not recurse into subfolders
- Does not auto-run `mem.py`
- Does not auto-run `__init__.py`
- Does not auto-run files beginning with `_`
- Launches scripts alphabetically
- Uses hidden process creation with `CreateProcessW` and `CREATE_NO_WINDOW`

The launcher tries:

1. `pythonw.exe`
2. `pyw.exe`
3. `py.exe -3`
4. `python.exe`

Python scripts receive these environment variables:

```text
SKATE_MOD_GAME_DIR
SKATE_MODS_DIR
SKATE_MOD_PY_DIR
SKATE_MEM_HOST=127.0.0.1
SKATE_MEM_PORT
```

## TCP Protocol

The protocol is newline-delimited JSON over localhost TCP.

Example ping request:

```json
{"cmd":"ping"}
```

Example response:

```json
{"ok":true,"data":"pong"}
```

Supported commands:

- `ping`
- `module_base`
- `read`
- `write`
- `read_abs`
- `write_abs`

Reads return hex strings. Writes accept hex strings. Memory access is limited to the current process that loaded the DLL.

`mem.py` typed helpers default to big-endian because Skate 3 values are normally big-endian:

```python
mem.write_float("game.exe", 0x123456, 1.0)
mem.write_uint_big_endian("game.exe", 0x123456, 123)
mem.write_uint_little_endian("game.exe", 0x123456, 123)
mem.write_int("game.exe", 0x123456, -1, endian="little")
```

## Troubleshooting

Check `GameFolder\python_runner.log` first.

- Python not found: install Python or add it to PATH. The DLL logs each launcher lookup result.
- No terminal should appear: scripts are launched with `pythonw.exe` when available and `CREATE_NO_WINDOW`.
- TCP port already in use: change `port` in `GameFolder\Mods\python_runner.ini`.
- Memory read/write failed: the address may be invalid, inaccessible, protected, or larger than the 4096 byte limit.
- Script failed to launch: check the logged command and Windows error.
- No success file: make sure `auto_launch_python=true`, Python is installed, and `test_script.py` exists in `GameFolder\Mods\py`.
