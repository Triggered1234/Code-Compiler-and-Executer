# Build Instructions

## Prerequisites

- CMake 3.16+
- GCC/G++ compiler
- Development libraries (see setup.sh)

## Build Types

### Release Build
```bash
./build.sh
```

### Debug Build with Tests
```bash
./build.sh -t Debug -c -T
```

### Full Development Build
```bash
./build.sh -t Debug -c -T -s --coverage
```

## Advanced Options

- `-s, --sanitizers`: Enable AddressSanitizer and UBSan
- `--coverage`: Enable code coverage analysis
- `--threading`: Enable ThreadSanitizer
- `-j N`: Use N parallel jobs

## Cross-platform Notes

The build system supports Linux, macOS, and can be adapted for Windows with MinGW.
