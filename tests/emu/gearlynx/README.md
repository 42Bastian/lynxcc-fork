# Gearlynx (headless) — sandbox ROM verification

A headless build of [Gearlynx](https://github.com/drhelius/Gearlynx) v1.2.14, an
accurate Atari Lynx emulator with an embedded **MCP server**. It runs inside the
Linux sandbox with no display, so built `.lnx` ROMs can be booted and inspected
(CPU, memory, Suzy/Mikey, screenshots) programmatically instead of by hand.

## Why a custom build

The official Linux release (`...-ubuntu24.04-arm64`) needs `GLIBC_2.38` /
`GLIBCXX_3.4.32`; the sandbox is Ubuntu 22.04 (glibc 2.35), so it won't run.
This binary is compiled from source against the system glibc and a privately
built headless **SDL3 3.4.10**. The sandbox is **ARM64**, so this is an aarch64
Linux ELF — it runs in the sandbox, *not* on macOS (use the Homebrew cask there).

## Layout

```
gearlynx        aarch64 Linux binary (stripped, git-ignored)
lib/            libSDL3.so* — headless SDL3 (git-ignored)
mcp/            MCP server resource files (hardware reference TOC)
lynxboot.img    Lynx BIOS, md5 fcd403db... (copyrighted, git-ignored — supply your own)
run.sh          launcher: writes config.ini + starts headless MCP server
```

## Usage

```bash
./run.sh /path/to/rom.lnx          # serves MCP on http://127.0.0.1:7777/mcp
GLYNX_PORT=7800 ./run.sh rom.lnx   # different port
```

Drive it with JSON-RPC over HTTP. The server must be launched and queried in the
**same shell session** (background processes don't survive across separate
sandbox bash calls). Minimal handshake then a tool call:

```bash
C="curl -s -H Content-Type:application/json -H Accept:application/json,text/event-stream http://127.0.0.1:7777/mcp"
$C -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"p","version":"1"}}}'
$C -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"get_6502_status","arguments":{}}}'
```

Useful tools: `debug_continue`, `debug_step_frame`, `debug_reset`,
`get_6502_status`, `read_memory`, `get_suzy_registers`, `get_mikey_registers`,
`get_screenshot` (returns a 160×102 PNG, base64), `get_frame_buffer`,
`get_sprite`, `set_breakpoint`. Full list via `tools/list`.

Audio init fails harmlessly (no audio device in the sandbox).

## Rebuild recipe

```bash
# cmake (no root): pip install --break-system-packages cmake
# SDL3 (headless):
git clone --depth 1 --branch release-3.4.10 https://github.com/libsdl-org/SDL.git SDL3
cmake -S SDL3 -B SDL3/build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$PWD/sdl3-prefix -DSDL_SHARED=ON -DSDL_STATIC=OFF \
  -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_UNIX_CONSOLE_BUILD=ON \
  -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_VULKAN=OFF -DSDL_DBUS=OFF \
  -DSDL_PULSEAUDIO=OFF -DSDL_ALSA=OFF -DSDL_PIPEWIRE=OFF -DSDL_LIBUDEV=OFF -DSDL_RPATH=OFF
cmake --build SDL3/build -j4 && cmake --install SDL3/build

# Gearlynx: drop "-lGL" from platforms/shared/makefiles/Makefile.common
#   (headless uses SDL_Init(0) and never touches GL; glad loads via -ldl)
git clone --depth 1 https://github.com/drhelius/Gearlynx.git
cd Gearlynx/platforms/linux
PKG_CONFIG_PATH=$PWD/../../../sdl3-prefix/lib/pkgconfig make DEBUG=1 -j4
strip gearlynx
```
