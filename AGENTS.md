## 1. Project Overview

This repository contains a custom ROM for the N64 game **GoldenEye 007** with various utilities for practicing speedruns. It is a fork of a work-in-progress, byte-matching decompilation of the original N64 game. It compiles C source files and assembly files back into `.z64` ROM binaries.

---

## 2. Directory Structure

Below is the high-level layout of the repository and where components are located:

```
goldeneye_src
├── assets/                  # Game assets extracted from baseroms (font, images, music, obseg models, briefs, text)
├── bin/                     # Raw binary components not yet matched or decompiled
├── build/                   # Compilation outputs and generated ROMs (e.g., build/u/ge007.u.z64)
├── include/                 # Game and engine header files (.h)
├── ld/                      # Memory layout/linker segment specification files (.ld.inc)
├── rsp/                     # Custom Reality Signal Processor (RSP) assembly microcode (C0 and 4Tri)
├── scripts/                 # Asset extraction, diffing, and environment helper scripts
├── src/                     # C and Assembly source files
│   ├── game/                # Core game-specific code (cheats, gun, player logic, objectives, text, etc.)
│   ├── practice/            # Utilities for practicing speedruns (new code goes here)
│   ├── inflate/             # Game decompression logic (zlib/inflate)
│   ├── libultra/            # N64 SDK library code
│   └── libultrare/          # N64 SDK library code modified by Rare
└── tools/                   # Build tools (SGI IRIX compiler, asm-processor, n64cksum, diff, asset utilities)
```

---

## 3. Build Commands

All builds must be run inside Docker since the host environment is macOS. Use the `goldeneye` Docker image to compile the ROMs.

### Standard Compilations

- **Build NTSC-US ROM (with Parallel Jobs)**:
    ```bash
    docker run --rm -v $(pwd):/home/dev goldeneye make -j8
    ```
- **Clean Build Files**:
    ```bash
    docker run --rm -v $(pwd):/home/dev goldeneye make clean
    ```

### Build Variables

You can configure Makefile behavior by passing the following flags (e.g., `make VARIABLE=VALUE`):

- `DEV`: `1` (disable optimizations, add dev code), `0` (prod build, default)
- `VERSION`: `US` (default), `JP`, `EU`
- `COMPARE`: `1` (compare ROM checksum against targets), `0` (disable comparison for custom builds, default)
- `IDO_RECOMP`: `YES` (default, uses fast natively compiled IDO C compiler), `NO` (uses QEMU to emulate the original IRIX binary)
- `FINAL`: `YES` (default), `NO` (builds non-working debug version of the decomp)
- `VERBOSE`: `0` (quiet, default), `1` (prints full command lines)

---

## 4. Guide to Key Source Files

When tasked with modification or analysis of gameplay, look here first:

- **Cheats & Inputs**:
    - `src/game/cheat_buttons.c` / `src/game/cheat_buttons.h`: Handles controller combination checks, cheat menu bindings, and cheat activations.
- **Practice**:
    - `src/practice`: Folder containing utilities for speedrun practice. All new logic goes in files here. This is not in the original game.
- **Weapon Logic**:
    - `src/game/gun.c` / `src/game/gun.h`: Massive file handling shooting, aiming, projectile calculations, reloading, and weapon state.
- **Player Physics & Camera**:
    - `src/game/player.c` / `src/game/player.h`: Player movement state machine, health, coordinates, and controls.
    - `src/game/bondview.c`: Heavy rendering, camera coordinates, and HUD logic.
- **Linker & Memory Map**:
    - `ge007.ld`: Top-level linker specification arranging code segments and overlays.
    - `ld/`: Segment definitions specifying BSS/text/rodata address overlays.

All new logic for the practice ROM is stored in the files under `src/practice`. If logic needs to be added to existing files, it should be added within an `#ifdef PRACTICE_ROM` block. If any lines are modified, the modified lines should exist in the `#ifdef` block with the original still existing, unmodified in the `#else` block. This is so that changes to the original code are easy to find and compare to original. `#include` directives do not need to be in `#ifdef` blocks.

---

## 5. Assembly & C Integration (GLOBAL_ASM)

The codebase heavily utilizes standard decompilation patterns to bridge C with yet-unmatched assembly code.

- If a function has not yet been decompiled to C, it is referenced via the preprocessor macro:
    ```c
    #pragma GLOBAL_ASM("asm/nonmatchings/game/some_file/some_func.s")
    ```
- The python preprocessor `tools/asm-processor/asm_processor.py` intercepts this during compile, compiles the `.s` file, and stabs the generated object code directly into the compiled C unit to allow matching builds.

---

## 6. Adding New Source Files

When adding a new `.c` file, Makefile will automatically pick it up but it must be added manually to the linker scripts at:

- `ld/game.text.ld.inc`
- `ld/game.data.ld.inc`
- `ld/game.bss.ld.inc`
- `ld/game.rodata.ld.inc`

---

## Debugging

1. Run the test case:
    - `just test TEST_CASE_NAME`
    - See practice_debug.c for the implementation of each test case
2. View logs
    - This will start the ares emulator and immediately do some action in a level
    - The test will emit logs to the emulator's STDOUT which you can use to see how far the test got and whether it succeeded or hung
    - It will also start a GDB server on localhost:9123 which you can connect to while the emulator is running
    - The emulator takes about 5 seconds to boot up and another 5 seconds to run the test case (depending on its implementation)
    - If needed you can use the `emu_log` function to log things to the emulator's STDOUT
    - If you stop this command the emulator will quit!
    - If you do run the emulator in the background remember to kill it later
3. Use the GDB MCP tools to connect
    - See the settings in `.vscode/launch.json` for how to connect
    - Note that the IDO compiler is old and does NOT produce debug symbols so only function breakpoints (not lines) work
    - You will also not be able to see variables and must look at the memory address pointed to by pointer arguments in the CPU registers if debugging game (not practice) code
    - However the newly added code in the src/practice files are compiled using GCC and DO have proper debug symbols so breaking and viewing variables there does work
