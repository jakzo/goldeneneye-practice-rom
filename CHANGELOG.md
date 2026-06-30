### Unreleased

# 0.4.0

- Save state support for NPCs, explosions/smoke, bullet/explosion marks and now adds/remove props on load
- Dam door guard load state indicator
- UI for selecting options when starting a level
- Fixed memory issues causing crash on load of some levels

# 0.3.0

- Added a splits system
    - Has some initial splits for Runway
- Fixed bug with sound being disabled for the save on first boot
- Save state support for doors and various prop types
- Switch level/difficulty in menu using L and R triggers

# 0.2.0

- Added mission timer display on screen during level
- Added hundredths to the mission end screen timer
- Rewriting state saving system to be more thorough
- Persistent save states (transitioned save data storage from EEPROM to SRAM)

# 0.1.0

Initial version. Not super useful yet since the main features are not finished yet but here it is. Adds features:

- [alpha] **Save states**
    - During gameplay press `L + D-Down` to save current state
    - During gameplay press `L + D-Up` to load current state
    - Note that many bits of state do not yet get restored, like killed guards and destroyed props
- [alpha] **Grenade camera**
    - When throwing grenades, shows a picture-in-picture view that follows the grenade so you can see where it landed
- **Skip level intro cutscenes**
    - Starting a level goes straight into it, rather than playing the intro cutscene
- **Skips legal and logo screens on game startup**
- [beta] **Time scale control**
    - During gameplay press `L + D-Left` to decrease game speed by 10%
    - During gameplay press `L + D-Right` to increase game speed by 10%
- **Save automatically fully unlocked**
    - On booting the ROM for the first time the default save will have all levels and cheats unlocked
