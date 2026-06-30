# Goldeneye N64 practice rom

This ROM provides tools for practicing speedruns of Goldeneye 007 on the Nintendo 64.

## How to install

- Download the latest `.xdelta` patch from [the releases page](https://github.com/jakzo/goldeneneye-practice-rom/releases)
- Acquire an unmodified base ROM of the US NTSC version of Goldeneye
- Use a ROM patcher tool such as [this one](https://www.marcrobledo.com/RomPatcher.js/) and apply the patch to the base ROM
- Run your modified ROM on:
    - Emulator:
        - Any should work though I recommend [ares](https://ares-emu.net/download) for its accuracy
    - Nintendo 64:
        - [SummerCart64](https://summercart64.dev/)
        - [EverDrive 64](https://krikzz.com/our-products/cartridges/ed64x7.html)
        - Copy the ROM to either of these via the micro SD card or USB

## Features

- **Timer displayed during levels**
- **End screen timer displays hundredths**
- **Time scale control**
    - During gameplay press `L + D-Left` to decrease game speed by 10%
    - During gameplay press `L + D-Right` to increase game speed by 10%
- **Skip level intro cutscenes**
    - Starting a level goes straight into it, rather than playing the intro cutscene
- **Skips legal and logo screens on game startup**
- **Save automatically fully unlocked**
    - On booting the ROM for the first time the default save will have all levels and cheats unlocked

### Level features

- **Dam**
    - Indicator for when the gate guard has loaded in
- **Runway**
    - Splits

### ℹ️ Beta features

These features have some minor bugs or limitations but should be usable:

- **Save states**
    - During gameplay press `L + D-Down` to save current state
    - During gameplay press `L + D-Up` to load current state
    - There are several minor visual/audio pieces of state which are not saved or loaded but should not affect gameplay
        - If there is some state not saved that DOES affect gameplay, let me know (@jakzo on Discord)
    - Not fully stable, expect occassional crashes
        - If you can reliably reproduce the crash, let me know so I can fix it (@jakzo on Discord)

### ⚠️ Alpha features

These features have many significant bugs and are unfinished. You should not expect them to work properly:

- **Grenade camera**
    - When throwing grenades, shows a picture-in-picture view that follows the grenade so you can see where it landed

## Development

See [readme-dev.md](readme-dev.md).
