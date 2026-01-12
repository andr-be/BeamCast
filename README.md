# BeamCast UT Simulator

A 2D ultrasonic testing simulator for educational and professional use.

## Prerequisites

- CMake 3.15 or higher
- C++17 compatible compiler (MSVC 2019+, GCC 9+, Clang 10+)
- SDL2 library

## Building on Windows

### 1. Install SDL2

Download SDL2 development libraries:
1. Go to https://github.com/libsdl-org/SDL/releases/latest
2. Download `SDL2-devel-2.x.x-VC.zip` (Visual C++ version)
3. Extract to `lib/SDL2/` in the project directory

Your structure should look like:
```
Project Confetti/
├── lib/
│   └── SDL2/
│       ├── include/
│       └── lib/
│           └── x64/
│               ├── SDL2.lib
│               ├── SDL2main.lib
│               └── SDL2.dll
├── src/
├── CMakeLists.txt
└── ...
```

### 2. Build with CMake

```bash
# Create build directory
mkdir build
cd build

# Configure (Visual Studio)
cmake ..

# Build
cmake --build . --config Release

# Run
bin\Release\BeamCast.exe
```

Alternatively, open the generated `.sln` file in Visual Studio.

## Building on Linux

### 1. Install SDL2

```bash
# Ubuntu/Debian
sudo apt-get install libsdl2-dev

# Fedora
sudo dnf install SDL2-devel

# Arch
sudo pacman -S sdl2
```

### 2. Build

```bash
mkdir build && cd build
cmake ..
make
./bin/BeamCast
```

## Controls

- **ESC**: Quit application

## Project Status

Currently in MVP development phase. See SPEC.md for full specification.

## License

TBD
