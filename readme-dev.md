The original README for the decompilation is at [readme-original.md](readme-original.md) and contains instructions for building, etc.

All new logic for the practice ROM is stored in the files under `src/practice`. If logic needs to be added to existing files, it should be added within an `#ifdef PRACTICE_ROM` block. If any code is modified, the new version should exist in the `#ifdef` block with the original still existing, unmodified in the `#else` block. This is so that changes to the original code are easy to find and compare to original.

To avoid distributing copyrighted material, only patch files are to be included in this repository and saved by CI for download. No ready-made ROMs of Goldeneye.
