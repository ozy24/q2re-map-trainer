import struct
import re
import os
import csv
import sys
import json

# ========= Item Mapping Lookup =========

# Cache for loaded mappings to avoid re-reading JSON file
_item_mappings_cache = None

def pause_if_executable():
    """Pause for user input if running as an executable"""
    if hasattr(sys, '_MEIPASS'):
        input("Press Enter to exit...")

def get_resource_path(relative_path):
    """Get absolute path to resource, works for dev and for PyInstaller"""
    try:
        # PyInstaller creates a temp folder and stores path in _MEIPASS
        base_path = sys._MEIPASS  # type: ignore
    except AttributeError:
        # Normal Python execution
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

def load_item_mappings():
    """Load item mappings from JSON file (cached)"""
    global _item_mappings_cache
    if _item_mappings_cache is None:
        try:
            json_path = get_resource_path('item_map.json')
            with open(json_path, 'r') as f:
                _item_mappings_cache = json.load(f)
        except FileNotFoundError:
            print("Error: item_map.json not found. Please ensure the file exists.")
            sys.exit(1)
        except json.JSONDecodeError:
            print("Error: Invalid JSON in item_map.json")
            sys.exit(1)
    return _item_mappings_cache

def get_item_info(class_name, game_version="Q2"):
    """Get item information from the JSON mappings"""
    mappings = load_item_mappings()
    game_data = mappings.get(game_version, {})
    
    # Search through all categories for the item
    for category in game_data.values():
        if class_name in category:
            return category[class_name]
    
    # Return default if not found
    return {"name": class_name, "type": "unknown"}

# ========= BSP Reader =========

def read_bsp_file(file_path):
    with open(file_path, 'rb') as f:
        header = f.read(4)
        version = struct.unpack('i', f.read(4))[0]

        expected_header = b'IBSP'
        expected_version = 38
        lump_count = 19

        if header != expected_header or version != expected_version:
            raise ValueError(f"Invalid Q2 BSP file: {file_path}")

        entries = []
        for _ in range(lump_count):
            offset, length = struct.unpack('ii', f.read(8))
            entries.append((offset, length))

        entity_offset, entity_length = entries[0]

        f.seek(entity_offset)
        entities_data = f.read(entity_length).decode('ascii', errors='ignore')

    return entities_data

# ========= Entity Parser =========

def parse_entities(entities_data):
    entities = []
    current_entity = {}
    worldspawn = None

    for line in entities_data.split('\n'):
        line = line.strip()
        if line == '{':
            current_entity = {}
        elif line == '}':
            if current_entity:
                if current_entity.get('classname') == 'worldspawn':
                    worldspawn = current_entity
                else:
                    entities.append(current_entity)
        elif line:
            match = re.match(r'"(.+)" "(.+)"', line)
            if match:
                key, value = match.groups()
                current_entity[key] = value

    return worldspawn, entities

# ========= Helpers =========

def extract_short_name(worldspawn):
    return worldspawn.get('message', 'Unknown')

def extract_items(entities):
    items = []
    for entity in entities:
        if 'classname' in entity and (
                entity['classname'].startswith('item_') or
                entity['classname'].startswith('weapon_') or
                entity['classname'].startswith('ammo_') or
                entity['classname'].startswith('holdable_')):
            origin = entity.get('origin', 'N/A')
            x, y, z = origin.split() if origin != 'N/A' else ('N/A', 'N/A', 'N/A')
            item_info = get_item_info(entity['classname'])
            items.append({
                'class_name': entity['classname'],
                'friendly_name': item_info['name'],
                'item_type': item_info['type'],
                'x': x,
                'y': y,
                'z': z
            })
    return items

def write_to_csv(items, csv_path, map_name=None, include_map_name_comment=True):
    with open(csv_path, 'w', newline='') as csvfile:
        # Optionally write map name as comment at the top
        if include_map_name_comment and map_name:
            csvfile.write(f"# Map: {map_name}\n")
        
        fieldnames = ['friendly_name', 'class_name', 'item_type', 'x', 'y', 'z']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for item in items:
            writer.writerow(item)

def ensure_dir(directory):
    if not os.path.exists(directory):
        os.makedirs(directory)

def to_snake_case(name):
    return re.sub(r'[^\w]', '_', name).lower()

# ========= Main BSP Processing =========

def process_bsp_file(bsp_file_path, csv_folder, use_simple_names=True, include_map_name_comment=True):
    entities_data = read_bsp_file(bsp_file_path)
    worldspawn, entities = parse_entities(entities_data)
    short_name = extract_short_name(worldspawn)
    items = extract_items(entities)

    bsp_filename = os.path.splitext(os.path.basename(bsp_file_path))[0]
    snake_case_name = to_snake_case(short_name)

    ensure_dir(csv_folder)

    # Choose filename based on use_simple_names setting
    if use_simple_names:
        csv_filename = f"{bsp_filename}.csv"
    else:
        csv_filename = f"{bsp_filename}_{snake_case_name}.csv"
    
    csv_path = os.path.join(csv_folder, csv_filename)

    # Pass the descriptive name for optional comment inclusion
    write_to_csv(items, csv_path, short_name if include_map_name_comment else None, include_map_name_comment)

    print(f"Processed: {bsp_file_path}")
    print(f"Short Name: {short_name}")
    print(f"Items found: {len(items)}")
    print(f"CSV file created: {csv_path}")
    if use_simple_names and include_map_name_comment:
        print(f"Map name '{short_name}' included as comment")
    print()

# ========= Main Program =========

def main():
    # For PyInstaller, use current working directory instead of script location
    if hasattr(sys, '_MEIPASS'):
        # Running as PyInstaller executable - use current working directory  
        base_dir = os.getcwd()
    else:
        # Running as Python script - use script directory
        base_dir = os.path.dirname(os.path.abspath(__file__))
    
    bsp_folder = os.path.join(base_dir, "maps")
    csv_folder = os.path.join(base_dir, "csv")

    ensure_dir(csv_folder)

    if not os.path.exists(bsp_folder):
        print(f"Error: maps folder not found!")
        pause_if_executable()
        return

    bsp_files = [f for f in os.listdir(bsp_folder) if f.endswith('.bsp')]

    if not bsp_files:
        print(f"No BSP files found in {bsp_folder}")
        pause_if_executable()
        return

    # Parse command line arguments
    use_simple_names = True  # Default to simple names (q2dm1.csv instead of q2dm1_the_edge.csv)
    include_map_name_comment = True  # Default to including map name as comment
    
    for arg in sys.argv[1:]:
        if arg == '--full-names':
            use_simple_names = False
        elif arg == '--no-comments':
            include_map_name_comment = False
        elif arg == '--help':
            print("Usage: python bsp_to_csv.py [--full-names] [--no-comments]")
            print("  --full-names: Use full descriptive names (q2dm1_the_edge.csv) instead of simple names (q2dm1.csv)")
            print("  --no-comments: Don't include map name as comment in CSV files")
            print("  Default: Simple names with map name comments")
            print("  Note: Only processes Quake 2 BSP files")
            pause_if_executable()
            return

    print(f"Settings:")
    print(f"  Simple names: {use_simple_names}")
    print(f"  Include comments: {include_map_name_comment}")
    print(f"  Game version: Quake 2 only")
    print()

    for bsp_file in bsp_files:
        bsp_file_path = os.path.join(bsp_folder, bsp_file)
        try:
            process_bsp_file(bsp_file_path, csv_folder, use_simple_names, include_map_name_comment)

        except Exception as e:
            print(f"Error processing {bsp_file_path}: {e}")

    print(f"All BSP files processed. CSV files can be found in: {csv_folder}")
    pause_if_executable()

if __name__ == "__main__":
    main()
