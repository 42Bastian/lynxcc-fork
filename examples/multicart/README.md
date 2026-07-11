# Multicart example

A **multicart** is a single `.lnx` cartridge image that bundles a **menu**
program together with several independent **game ROMs**. At power-on the SDK
bootloader runs the menu; when the player picks a game, the menu calls
`multicart_run(n)` to load that game off the cartridge — over the top of the
menu — and run it.

This example builds a menu plus three tiny "games" (they just show a big `1`,
`2` or `3`) and stitches them into one bootable `multicart.lnx`.

## How it fits together

The cartridge holds three kinds of code:

1. **The menu** (`menu.c`) — a normal Lynx program that draws the game list and
   calls `multicart_run()` when the player presses A.
2. **The games** (`game1.c`, `game2.c`, `game3.c`) — complete, independent Lynx
   programs. The menu knows nothing about them beyond their slot number.
3. **The runtime loader** — a small relocatable routine baked into the SDK
   (`runtime/lynx/multicartldr.s`). `multicart_run()` copies it to `$0040` and
   jumps to it; it reads the chosen game off the cartridge and starts it.

The menu and every game are compiled as **BLL objects** (`cl65 -C lynx-bll.cfg`),
the object format the ROM builder understands. `lnx multicart` writes a `lynxdir`
`.mak` describing the layout, and `lynxdir` assembles the final cart:

```
cl65 -C lynx-bll.cfg  →  menu.o, game1.o, game2.o, game3.o
lnx multicart         →  multicart.mak
lynxdir multicart.mak →  multicart.lnx  (+ multicart.lyx)
```

## Build and run

```
make            # builds multicart.lnx
```

Boot `multicart.lnx` on a Lynx or emulator. Use up/down to move the cursor and
**A** to launch a game. Launching a game overwrites the menu, so returning to the
menu requires a reboot.

See `design/LYNX_MULTICART_DESIGN.md` and `doc/multicart.html` for the full
mechanism, the directory layout and the `multicart_run()` API.
