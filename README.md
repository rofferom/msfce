# Another Super Nintendo emulator

## About

This is my first attempt to write an emulator, I have always been curious about SNES internals, and how an emulator could be written.

The implementation is not complete, there are glitches/crashes in games, but the emulator manages to run many games.

It runs on Linux and Windows.

## Usage

The emulator currently doesn't provide a GUI. It must be runned in CLI:
```
./msfce <rom>
```

More details available about usage:
```
./msfce --help
```

## Project state

### SNES feature implementation status

* CPU implementation seems to work
* APU implementation is [Blargg's one](https://www.slack.net/~ant/). The link on its website is broken but there is a [copy available on github](https://github.com/elizagamedev/snes_spc). I didn't want to waste time to increase the compatibility of my dummy APU implementation, and it's way better to test games with sound
* PPU implementation
  * Implemented
    * Mode 0
    * Mode 1
    * Mode 3
    * Mode 7
    * MainScreen/SubScreen, Window, Color math
    * Mosaic
    * Controller (Only player 1). Both automatic and manual read
  * Not implemented
    * Mode 2
    * Mode 5
    * Mode 6
    * Enhancement chips (DSP1, SuperFX, ...)
* Rom types: LowRom, HighRom. Heuristic to detect rom type has been imported from [bsnes](https://github.com/bsnes-emu/bsnes).
* SRAM save on disk
* NTSC support only

### Other features

* Save state
* Screenshot
* Video/audio recording

### Performances

Performances could be really better. However, it runs on a decent machine at almost fullspeed. The primary goal of the project is to get something that works. I'll work on speed later.

### Compatibility

The tests have mainly been done on two games
* Super Mario World
* Legend of Zelda: A Link to the Past

Extra debug efforts have been done to make more games intro work
* Bahamut Lagoon
* Donkey Kong Country
* Donkey Kong Country 2
* Donkey Kong Country 3
* Dragon Quest I & II
* Dragon Quest III
* Dragon Quest V
* Dragon Quest VI
* Final Fantasy II (US)
* Final Fantasy III (US)
* Final Fantasy V (J)
* F-Zero
* Mega Man X
* Gradius III
* R-Type III
* Super Castlevania IV
* Super Mario All Stars
* Super Metroid

## Controls

### Keyboard

| Keyboard scancode | SNES controller |
|-------------------|-----------------|
| Arrow Up          | Up              |
| Arrow Down        | Down            |
| Arrow Left        | Left            |
| Arrow Right       | Right           |
| Q                 | L               |
| W                 | R               |
| Shift             | Select          |
| Enter             | Start           |
| Z                 | B               |
| X                 | A               |
| A                 | Y               |
| S                 | S               |

Those controls are [RetroArch's default ones](https://docs.libretro.com/guides/input-and-controls/#default-retroarch-keyboard-bindings).

### Gamepad

Gamepad are supported. Tested with an Xbox 360 USB controller, and a Switch Pro Controller.

### Extra shortcuts

| Keyboard scancode | Action              |
|-------------------|---------------------|
| P                 | Pause               |
| O                 | Toggle video record |
| F8                | Screenshot          |
| F2                | Create savestate    |
| F4                | Load savestate      |

## Compile

### Get submodules (all platforms)

```
git submodule update --init
```

### Linux

Tested on Debian 11 and Ubuntu 20.20.

Install required packages
```
sudo apt install build-essential cmake pkg-config \
  libsdl2-dev libepoxy-dev libglm-dev \
  libavcodec-dev libavutil-dev libavformat-dev libswscale-dev
```

Build
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make
```

### Windows

Only cross compilation from Linux is supported. Tested on Debian 11 and Ubuntu 20.20.

Install required packages
```
sudo apt install mingw-w64 cmake pkg-config wget unzip \
  meson ninja-build nasm
```

Build dependencies
```
export PKG_CONFIG_LIBDIR=$(pwd)/mingw64/x86_64-w64-mingw32/lib/pkgconfig
./mingw64/build_deps.sh
```

Build
```
export PKG_CONFIG_LIBDIR=$(pwd)/mingw64/x86_64-w64-mingw32/lib/pkgconfig
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../mingw64/toolchain.cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Ressources

* [65C816 Opcodes by Bruce Clark](http://6502.org/tutorials/65c816opcodes.html)
* [nocash SNES documentation](https://problemkaputt.de/fullsnes.htm)
* [SNES development Wiki](https://wiki.superfamicom.org)
* [Snes Programming Wikibook](https://en.wikibooks.org/wiki/Super_NES_Programming)
* When documentation wasn't enough, I had a look on [bsnes](https://github.com/bsnes-emu/bsnes), [snes9x](https://github.com/snes9xgit/snes9x) and [LakeSnes](https://github.com/elzo-d/LakeSnes).
* [PeterLemon's SNES test Roms](https://github.com/PeterLemon/SNES)