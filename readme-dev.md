## Quick start

- Copy the US version of the Goldeneye ROM to `baserom.u.z64` in the repo root
- Install and run Docker
- Run `docker build -t goldeneye .`
- Extract assets from the ROM with `docker run --rm -v $(pwd):/home/dev goldeneye ./scripts/extract_baserom.u.sh`
- Build with `docker run --rm -v $(pwd):/home/dev goldeneye make -j8 COMPARE=0 FINAL=NO`
    - For future builds just run this command again, the others were one-off setup
- The built ROM will be at `build/u/ge007.u.z64`

## Development

The original README for the decompilation is at [readme-original.md](readme-original.md) and contains instructions for building, etc.

All new logic for the practice ROM is stored in the files under `src/practice`. If logic needs to be added to existing files, it should be added within an `#ifdef PRACTICE_ROM` block. If any lines are modified, the modified lines should exist in the `#ifdef` block with the original still existing, unmodified in the `#else` block. This is so that changes to the original code are easy to find and compare to original. `#include` directives do not need to be in `#ifdef` blocks.

When adding new features or making significant changes, make sure to document it in the [CHANGELOG.md](./CHANGELOG.md).

To avoid distributing copyrighted material, only patch files are to be included in this repository and saved by CI for download. No ready-made ROMs of Goldeneye.

## Debugging

Easiest way is to log to the screen with `practiceLogDebug`.

However if you use an emulator like [ares](https://ares-emu.net/download) you can start a GDB server and compiling with `FINAL=NO` will include debug symbols, so you can use GDB to attach a debugger and step through the code. Full command is:

```sh
docker run --rm -v $(pwd):/home/dev goldeneye make -j8 COMPARE=0 FINAL=NO
```

**But this is not working right now.** For some reason it only shows a black screen on startup and I haven't figured out why yet.

However if you want to run on the console this is missing optimizations and there is no need for debugging symbols so using `FINAL=YES` (default) then you can compile a more console-friendly build:

```sh
docker run --rm -v $(pwd):/home/dev goldeneye make -j8 COMPARE=0
```

If something goes wrong and you're getting mysterious build errors, clean and start fresh with:

```sh
docker run --rm -v $(pwd):/home/dev goldeneye make nuke && \
docker run --rm -v $(pwd):/home/dev goldeneye ./scripts/extract_baserom.u.sh && \
docker run --rm -v $(pwd):/home/dev goldeneye make -j8 COMPARE=0
```

I find I need to do this when switching between building with `FINAL=YES` and `FINAL=NO`.

## Release

All pushes to master branch trigger Github actions to build a patch and release it as a prerelease.

To trigger a full version release, update [CHANGELOG.md](./CHANGELOG.md) to add a new entry for your version, then manually trigger the release action and it will pull the latest entry from the CHANGELOG.md and use that version in the name and the contents as the release body.
