# Building TXD Editor

## Prerequisites

### macOS (using Homebrew)

```bash
# Install CMake
brew install cmake

# Install Qt6 (recommended) or Qt5
brew install qt@6
# OR
brew install qt@5
```

After installing Qt via Homebrew, you may need to set the CMAKE_PREFIX_PATH:

```bash
export CMAKE_PREFIX_PATH=$(brew --prefix qt@6)
# OR for Qt5:
# export CMAKE_PREFIX_PATH=$(brew --prefix qt@5)
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install cmake qt6-base-dev qt6-base-dev-tools
# OR for Qt5:
# sudo apt-get install cmake qt5-default qtbase5-dev
```

### Windows

1. Install CMake from https://cmake.org/download/
2. Install Qt from https://www.qt.io/download
3. Make sure Qt's bin directory is in your PATH

## Building

```bash
mkdir build
cd build
cmake ..
make
```

On Windows with Visual Studio:

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
```

## Running

After building, the executable will be in:

- Linux/macOS: `build/bin/txdedit`
- Windows: `build/bin/Release/txdedit.exe`

## Troubleshooting

### CMake can't find Qt

If CMake can't find Qt, specify the path explicitly:

```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt
```

On macOS with Homebrew:

```bash
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
```
