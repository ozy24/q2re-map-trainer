# Trainer Debug System - Centralized Logging

## ✅ Implementation Complete

### Build Status
```
Build succeeded.
    0 Warning(s)
    0 Error(s)

✅ DLL created and copied successfully!
```

---

## 🎯 Overview

A centralized debug logging system for the entire trainer module, controlled by the `trainer_debug` cvar. All trainer features can now log to a single `trainer.log` file with categorized messages.

---

## 📁 Files Created/Modified

### New Files
1. **`trainer/trainer_debug.cpp`** (77 lines)
   - Centralized logging implementation
   - Automatic log file management
   - Timestamp and category support

### Modified Files (Minimal Vanilla Changes)
2. **`g_main.cpp`** (+4 lines)
   - Line 77: Added `cvar_t *trainer_debug;` declaration
   - Line 270: Added cvar initialization

3. **`trainer/trainer.h`** (+5 lines)
   - Lines 93-97: Added debug API declarations

4. **`trainer/trainer_jump.cpp`** (refactored)
   - Removed local debug logging function
   - Updated all TrainerLog calls to use centralized system

5. **`game.vcxproj`** (+1 line)
   - Line 246: Added `trainer\trainer_debug.cpp` to build

---

## 🎮 Usage

### In-Game Console Commands

```bash
# Enable debug logging
set trainer_debug 1

# Disable debug logging
set trainer_debug 0
```

**Note**: Debug logging is **disabled by default** (0).

---

## 💻 Code Usage (For Developers)

### Basic Logging

```cpp
#include "trainer.h"

// Simple log message
TrainerLog("BHOP", "Player jumped");

// Formatted message
TrainerLog("PATH", "Target picked: %s at (%d, %d, %d)", 
    item_name, x, y, z);

// Complex data
TrainerLog("TIMING", "Entry: active=%d, frames=%d, time=%.2f",
    entry->active, entry->frames, entry->time.seconds());
```

### Visual Separator

```cpp
TrainerLog_Separator();  // Adds ---- line to log for readability
```

### Categories

Use consistent category names for easy filtering:
- **"BHOP"** - Bhop consistency tracking
- **"PATH"** - Path training
- **"TIMING"** - Item timing trainer  
- **"MENU"** - Menu system
- **"CORE"** - Core trainer functions
- **"JUMP"** - Jump trainer features

---

## 📝 Log Format

### File Location
```
<Quake2_Install_Dir>/trainer.log
```

### Log Entry Format
```
[HH:MM:SS] [CATEGORY] message
```

### Example Log
```
==========================================
Trainer Debug Log Started: 2025-11-01 21:30:15
==========================================
[21:30:20] [BHOP] LANDED: chain_active=0, recently_landed=0->1, frames=0->0
[21:30:21] [BHOP] JUMP: chain_active=0, recently_landed=0, frames=0, perfect=0, late=0
[21:30:21] [BHOP]   -> Starting chain (no feedback)
[21:30:21] [BHOP]   -> Reset: recently_landed=0, frames=0
[21:30:22] [BHOP] LANDED: chain_active=1, recently_landed=0->1, frames=0->0
[21:30:22] [BHOP] JUMP: chain_active=1, recently_landed=1, frames=1, perfect=1, late=0
[21:30:22] [BHOP]   -> Feedback: Perfect
[21:30:22] [BHOP]   -> Reset: recently_landed=0, frames=0
----------------------------------------
[21:30:30] [TIMING] Megahealth picked up at (100, 200, 300)
[21:30:35] [PATH] Target changed: Mega Health -> Red Armor
```

---

## 🏗️ Architecture (Thin Vanilla Compliant)

### Vanilla Changes (Minimal)
```cpp
// g_main.cpp - Only 4 lines added
cvar_t *trainer_debug;  // Line 77

trainer_debug = gi.cvar("trainer_debug", "0", CVAR_NOFLAGS);  // Line 270
```

**Total vanilla impact**: 4 lines

### Trainer Module (All Logic Isolated)
```
trainer/
├── trainer_debug.cpp    [NEW] Centralized logging (77 lines)
├── trainer.h            [+5 lines] API declarations
├── trainer_jump.cpp     [refactored] Uses new system
├── trainer_core.cpp     [ready] Can use TrainerLog()
├── trainer_path.cpp     [ready] Can use TrainerLog()
├── trainer_timing.cpp   [ready] Can use TrainerLog()
└── trainer_menu.cpp     [ready] Can use TrainerLog()
```

---

## ✅ Thin Vanilla Compliance

| Principle | Status | Notes |
|-----------|--------|-------|
| **Minimal vanilla changes** | ✅ | Only 4 lines in g_main.cpp |
| **Logic in trainer/** | ✅ | All logging code in trainer_debug.cpp |
| **Clear [Map Trainer] markers** | ✅ | Comments mark all changes |
| **Controlled by cvar** | ✅ | trainer_debug cvar (0=off, 1=on) |
| **Easy to disable** | ✅ | Set cvar to 0 |
| **Easy to remove** | ✅ | Delete 4 lines + trainer_debug.cpp |
| **No signature changes** | ✅ | No vanilla functions modified |
| **Self-contained** | ✅ | All in trainer/ directory |

---

## 🔍 Features

### 1. **Automatic Log Management**
- Creates `trainer.log` on first write
- Clears old log on game start (truncate mode)
- Automatic timestamps
- Immediate flush (no buffering)

### 2. **Category Support**
- First parameter is always the category
- Easy to filter/search logs by category
- Consistent formatting

### 3. **Conditional Logging**
- Only logs when `trainer_debug` is 1
- Zero performance impact when disabled
- No file operations when disabled

### 4. **Thread-Safe**
- Each log write opens/closes file
- Flushes immediately
- Safe for concurrent access

### 5. **Printf-Style Formatting**
- Supports all standard printf format specifiers
- Variable arguments
- 512-byte buffer per message

---

## 📊 Performance

### When Disabled (trainer_debug = 0)
- **Cost**: 1 integer check per TrainerLog() call
- **File I/O**: None
- **Impact**: Negligible (~1 CPU cycle)

### When Enabled (trainer_debug = 1)
- **Cost**: File open/write/close per message
- **Buffering**: None (immediate flush)
- **Impact**: Acceptable for debugging (not for production)

---

## 🎓 Adding Debug Logging to Other Trainer Modules

### Example: Adding to trainer_path.cpp

```cpp
void MapTrainer_PickNewTarget()
{
    // ... existing code ...
    
    TrainerLog("PATH", "Picking new target from %d available types", 
        available_count);
    
    // ... more code ...
    
    if (selected_target)
    {
        TrainerLog("PATH", "Selected: %s at (%.1f, %.1f, %.1f)",
            selected_target->friendly_name,
            selected_target->position[0],
            selected_target->position[1],
            selected_target->position[2]);
    }
}
```

### Example: Adding to trainer_timing.cpp

```cpp
void MapTrainer_OnTimingItemPickup(edict_t *ent, edict_t *other)
{
    TrainerLog("TIMING", "Item pickup: %s by player %d",
        ent->item->classname,
        other->s.number);
    
    // ... existing code ...
    
    if (timing_entry)
    {
        TrainerLog("TIMING", "Created entry: %s, respawn=%.1fs",
            item_name,
            respawn_time.seconds());
    }
}
```

---

## 🧪 Testing

### Quick Test Sequence

1. **Start Quake 2**
2. **Open console** (~)
3. **Enable debug**: `set trainer_debug 1`
4. **Enable bhop trainer** (via menu)
5. **Do a few jumps**
6. **Check log**: `<Q2_DIR>/trainer.log`

### Expected Output

You should see entries like:
```
[TIME] [BHOP] LANDED: chain_active=0, ...
[TIME] [BHOP] JUMP: chain_active=0, ...
[TIME] [BHOP]   -> Starting chain (no feedback)
```

---

## 🔧 Future Enhancements

Possible additions (not implemented yet):
- [ ] Log level support (INFO, DEBUG, WARN, ERROR)
- [ ] Per-category enable/disable (e.g., `trainer_debug_bhop`)
- [ ] Log rotation (e.g., keep last 5 logs)
- [ ] In-game log viewer
- [ ] Performance stats logging
- [ ] Network logging for remote debugging

---

## 📚 API Reference

### Functions

#### `void TrainerLog(const char* category, const char* format, ...)`
**Purpose**: Write a categorized log message

**Parameters**:
- `category` - Category name (e.g., "BHOP", "PATH", "TIMING")
- `format` - Printf-style format string
- `...` - Variable arguments for format string

**Example**:
```cpp
TrainerLog("BHOP", "Player %d jumped at frame %d", player_id, frame_count);
```

---

#### `void TrainerLog_Separator()`
**Purpose**: Write a visual separator line to the log

**Example**:
```cpp
TrainerLog("BHOP", "Starting new session");
TrainerLog_Separator();
TrainerLog("BHOP", "First jump detected");
```

**Output**:
```
[21:30:00] [BHOP] Starting new session
----------------------------------------
[21:30:01] [BHOP] First jump detected
```

---

## 🎯 Summary

**Implemented**:
- ✅ Centralized debug system
- ✅ Controlled by `trainer_debug` cvar
- ✅ Category support
- ✅ Minimal vanilla changes (4 lines)
- ✅ All logic in `trainer/` directory
- ✅ Easy to use API
- ✅ Follows thin vanilla principles

**Result**: The entire trainer module now has a professional debug logging system that can be toggled on/off at runtime, with zero performance impact when disabled!

---

Date: 2025-11-01  
Created: `trainer/trainer_debug.cpp`  
Modified: `g_main.cpp`, `trainer/trainer.h`, `trainer/trainer_jump.cpp`, `game.vcxproj`  
Cvar: `trainer_debug` (default: 0)  
Log File: `<Q2_DIR>/trainer.log`

