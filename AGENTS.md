# This game

A Game Boy Color game for the ModRetro Chromatic, written in C with GBDK-2020 (version 4.5).
Vibe Retro Studio compiles it after every change and shows it on the 160×144 screen.

## The hardware cannot grow

These limits are fixed. Never work around them, and say so plainly when a request cannot fit.

| Limit | Value |
| --- | --- |
| Screen | 160×144 pixels, 20×18 tiles |
| Colours | 4 per tile, 8 background and 8 sprite palettes |
| Sprites | 40 on screen, 10 per scanline, 8×8 tiles |
| Cartridge | 2,048 KB ROM at most (MBC5), 32 KB save RAM |

A build above 2,048 KB is refused by the app and never runs.

## How to work

- All game code lives in `src/`. Add `.c` files there and they are built automatically.
- Graphics are tile arrays in `src/art.c`, with their numbers in `src/art.h`.
- Do not run the compiler yourself. The app builds after your turn and sends you any errors.
- Do not commit. The app commits after a successful build, and the maker pushes.
- Keep to the button set of the handheld: D-pad, A, B, Start and Select.
- Draw during vertical blank where it matters, and keep frame work small: this is a 4 MHz CPU.
