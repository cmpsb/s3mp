# s3mp

A player for [ScreamTracker 3 modules](https://en.wikipedia.org/wiki/S3M_(file_format)).

## Features

* Very simple, written in modern C
* Support for 8-bit and 16-bit PCM samples
* Has pretty colors
* Works OK on a Raspberry Pi 1B

## Installation

s3mp requires libsamplerate, SDL2 and SDL2_mixer. On Debian, the corresponding packages are `libsamplerate0-dev`, `libsdl2-dev` and `libsdl2-mixer-dev`. Compiling requires `cmake` version 3.7+ and a reasonably modern GCC, preferably 9 or later, or a compatible compiler such as Clang.

Compiling should be done out-of-source:
```sh
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

The `s3mp` executable is placed in the build directory.

## Usage

**Warning: the player may be very loud!**  
Use your system's volume controls to control s3mp's volume. There is no way to modify s3mp's volume from s3mp itself.

Given a file, for example `PELIMUSA.S3M` in the build directory:

```sh
./s3mp PELIMUSA.S3M
```

The player will build up a note cache by playing the song very fast once. This process audible because it sounds funny.

During normal playback, the player will emulate the [usual tracker output](https://en.wikipedia.org/wiki/Music_tracker). You can exit the program using Ctrl+C.

The program will disable text wrapping on the terminal. It does not restore wrapping, and many shells don't either. It's best to just open a new terminal window.
