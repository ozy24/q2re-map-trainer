# Quake 2 Rerelease Map Trainer

A training mod for Quake 2 Rerelease that helps players learn new maps and practice jumps.

## What it does

- **Item Training**: Highlights specific items on the map for you to collect in sequence
- **Spawn Management**: Save and warp to custom spawn points for jump practice
- **Speed Tracking**: Optional speedometer to monitor your movement

For item training, item location data must be pulled from the map (.bsp) file and converted to .csv file using the bsp2csv tool.  There are a number of map .csv files ready to go, but in the event you need to learn a new map the conversion process is simple.

## Installation

1. **Download** the latest release zip file
2. **Extract** the maptrain folder into your Q2RE rerelease folder
   for example:  "c:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\"
3. **Launch** Quake 2 and type the command "game maptrain" at the console. Now load a map to activate the mod.

## Usage

- **Open trainer menu**: Press tab
- **Set spawn point**: Type `setspawn` to save your current position
- **Warp to spawn**: Type `warpspawn` to return to your saved position
- **Configure categories**: Use the menu to enable/disable item types and speedometer for training

## Supported Maps

The conversion tool should work with all maps but please let me know if one does not!