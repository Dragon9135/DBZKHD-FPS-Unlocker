# FPS Unlocker for DBZK (HD)

A simple ASI mod that unlocks the FPS limit for Dragon Ball Z: Kakarot (HD Version). It automatically locks the game’s FPS to your monitor’s refresh rate.

## Features

- Automatically detects your monitor’s refresh rate (e.g., 60, 120, 144 Hz)
- Updates the in-game FPS value accordingly
- Requires no additional settings or configuration

## Installation

1. Make sure [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) or a similar ASI loader is installed in the game’s main directory.
2. Copy the `FPS Unlocker for DBZK (HD).asi` file to the folder containing the game’s `AT-Win64-Shipping.exe` file.
3. Launch the game.

If you'd like, you can download the latest version of the Ultimate ASI Loader's .DLL file directly from [here](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/winmm-x64.zip).

## Notes

- Compatible with version 1.40 (HD) of the game.
- The mod takes effect a few seconds after the game starts.
- A huge thank you to Gantz79. If it weren't for the [Cheat Engine Table](https://www.nexusmods.com/dragonballzkakarot/mods/527) he created, I wouldn't have been able to make this mod.
- I compiled it using the MinGW-w64 library.
- I used commands `x86_64-w64-mingw32-windres Version.rc -O coff -o Version.res` and `x86_64-w64-mingw32-g++ -shared -o "FPS Unlocker for DBZK (HD).asi" "FPS_Unlocker_for_DBZK__HD_.cpp" Version.res -O2 -static -static-libgcc -static-libstdc++` to compile the ASI file.
- AI assistance was used for some functions and bug fixes.

## Contributors

- [Gantz79](https://www.nexusmods.com/profile/Gantz79)
- [Talha2003](https://www.nexusmods.com/profile/Talha2003)

## Disclaimer

This mod is for entertainment purposes only and is provided “as is.” Use at your own risk.
