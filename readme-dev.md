## Quick start

We use a [`justfile`](https://github.com/casey/just)!

```sh
# just run `just` by itself to see all available commands
just

# only need to run this once to setup the repository
just setup

# builds the rom, and uploads it to a connected summercart64
just sc64
```

## Development

The original README for the decompilation is at [readme-original.md](readme-original.md) and contains instructions for building, etc.

All new logic for the practice ROM is stored in the files under `src/practice`. If logic needs to be added to existing files, it should be added within an `#ifdef PRACTICE_ROM` block. If any lines are modified, the modified lines should exist in the `#ifdef` block with the original still existing, unmodified in the `#else` block. This is so that changes to the original code are easy to find and compare to original. `#include` directives do not need to be in `#ifdef` blocks.

When adding new features or making significant changes, make sure to document it in the [CHANGELOG.md](./CHANGELOG.md).

To avoid distributing copyrighted material, only patch files are to be included in this repository and saved by CI for download. No ready-made ROMs of Goldeneye.

## Debugging

Easiest way is to log to the screen with `practiceLogDebug`. These will also log to STDOUT of supported emulators (such as ares). If you want to only log to STDOUT you can use `emu_log_write`.

If you use an emulator like [ares](https://ares-emu.net/download) you can start a GDB server and compiling with `FINAL=NO` will include debug symbols, so you can use GDB to attach a debugger. Full command is:

```sh
docker run --rm -v $(pwd):/home/dev goldeneye make -j8 COMPARE=0 FINAL=NO
```

But there are some limitations:

- It often compiles to a ROM which only shows a black screen on startup
    - Need to run the full fresh build command below to fix
- The compiler does not support DWARF debug symbols so no line breaks

Also keep in mind that this build is missing optimizations which you get with `FINAL=YES` (default):

```sh
docker run --rm -v $(pwd):/home/dev goldeneye make -j8 COMPARE=0
```

If something goes wrong and you're getting mysterious build errors, clean and start fresh with:

> **Warning:** this will delete any uncommitted or ignored files inside the repo directory!

```sh
git clean -fdx -e '.vscode/*' -e 'baserom.u.z64' && \
docker image rm goldeneye && \
docker build -t goldeneye . && \
docker run --rm -v $(pwd):/home/dev goldeneye ./scripts/extract_baserom.u.sh && \
docker run --rm -v $(pwd):/home/dev goldeneye make -j8 COMPARE=0 FINAL=YES
```

I find I need to do this when switching between building with `FINAL=YES` and `FINAL=NO`.

I also find that ares needs a restart from time and time. I've spent a lot of time ripping my hair out trying to find out why something hangs in a certain situation, only to find out it works after restarting ares!

I use this on Mac to run in ares:

```sh
/Applications/ares.app/Contents/MacOS/ares --system "Nintendo 64" ./build/u/ge007.u.z64
```

You can also build it to start in a specific level:

```sh
docker run --rm -v $(pwd):/home/dev goldeneye make -j8 COMPARE=0 BOOT_LEVEL=LEVELID_RUNWAY
```

## Release

All pushes to master branch trigger Github actions to build a patch and release it as a prerelease.

To trigger a full version release, update [CHANGELOG.md](./CHANGELOG.md) to add a new entry for your version, then manually trigger the release action and it will pull the latest entry from the CHANGELOG.md and use that version in the name and the contents as the release body.
