# Final Fantasy XV Fix
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/PolarWizard/FinalFantasyXVFix/total)

***This project is designed exclusively for Windows due to its reliance on Windows-specific APIs. The build process requires the use of PowerShell.***

## Enhancements
- Centers and constrains HUD to 16:9
- Uncaps FPS

## Build and Install
### Using CMake
1. Build and install:
```ps1
git clone https://github.com/PolarWizard/FinalFantasyXVFix.git
cd FinalFantasyXVFix; mkdir build; cd build
# If install is not needed you may omit -DCMAKE_INSTALL_PREFIX and cmake install step.
cmake -DCMAKE_INSTALL_PREFIX=<FULL-PATH-TO-GAME-FOLDER> ..
cmake --build . --config Release
cmake --install .
```
2. Download [d3d11.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases) Win64 version
3. Extract to game folder: `FINAL FANTASY XV`

### Using VSCode Task
1. `Ctrl+Shift+P`
2. `>Tasks: Run Task`
3. Run task group `configure, build, and install` or individually `configure`, `build`, and `install`

### Using Release
Download and follow instructions in [latest release](https://github.com/PolarWizard/FinalFantasyXVFix/releases)

## Configuration
- Adjust settings in `FINAL FANTASY XV/scripts/FinalFantasyXVFix.yml`

## Screenshots
| ![Demo1](images/FinalFantasyXVFix_1.gif) |
| --- |
| <p align='center'> Vanilla → Modded </p> |

## Licenses
Distributed under the MIT License. See [LICENSES](LICENSES) for more information.

## External Tools
- [safetyhook](https://github.com/cursey/safetyhook)
- [spdlog](https://github.com/gabime/spdlog)
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
- [yaml-cpp](https://github.com/jbeder/yaml-cpp)
- [zydis](https://github.com/zyantific/zydis)
