# Quake 2 Rerelease Map Trainer

A training mod for Quake 2 Rerelease that helps players learn new maps and practice jumps.

## What it does

- **Item Training**: Highlights specific items on the map for you to collect in sequence
- **Item Timing**: Starts a timer when picking up a weapon, armor or powerup and provides feedback to players on timing accuracy
- **Spawn Management**: Save and warp to custom spawn points for jump practice
- **Speed Tracking**: Optional speedometer to monitor your movement

For item training, item location data must be pulled from the map (.bsp) file and converted to .csv file using the bsp2csv tool.  There are a number of map .csv files ready to go, but in the event you need to learn a new map the conversion process is simple.

## Installation

1. **Download** the latest release zip file
2. **Extract** the trainer folder into your Q2RE rerelease folder
   for example:  "c:\Program Files (x86)\Steam\steamapps\common\Quake 2\rerelease\"
3. **Launch** Quake 2 and type the command "game trainer" at the console. Now load a map to activate the mod.

## Usage

- **Open trainer menu**: Press tab
- **Set spawn point**: Type `savepos` to save your current position (recommended to bind to a key)
- **Warp to spawn**: Type `loadpos` to return to your saved position (recommended to bind to a key)
- **Configure categories**: Use the menu to enable/disable item types and speedometer for training

## Converting maps

Navigate to the trainer folder and and locate bsp_to_csv.exe.  Place the map you want to train on in the /maps subfolder then run bsp_to_csv.exe (or bsp_to_csv.py if you have python installed).

This will generate a corresponding csv file in the /csv folder.  That's it! You can now begin training on that map.

## Supported Maps

The conversion tool should work with all maps but please let me know if one does not!