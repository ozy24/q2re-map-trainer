# OzMod "Thin Vanilla" Design Principles

## Philosophy
OzMod follows a **"thin vanilla"** architecture where:
- Minimal changes to the vanilla Quake II codebase
- Maximum functionality contained in the `/ozmod` directory
- Clear separation between vanilla and OzMod code
- Easy to identify, maintain, and potentially remove OzMod features

## Benefits

### 1. Maintainability
- Vanilla updates can be merged with minimal conflicts
- OzMod features are self-contained and easy to understand
- Clear ownership of code sections

### 2. Debuggability
- Bugs are easier to isolate (vanilla vs OzMod)
- Features can be individually disabled for testing
- Changes are traceable with clear `[OzMod]` markers

### 3. Community Acceptance
- Respects the original codebase
- Minimal "pollution" of vanilla code
- Easy for others to audit or remove features

### 4. Flexibility
- Features can be easily enabled/disabled via cvars
- Individual features can be removed without affecting others
- Easy to port features to other Quake II mods

## Implementation Guidelines

### ✅ DO: Keep Vanilla Changes Minimal

**Good Example** - `om_map_shuffle_once`:
```cpp
// In g_main.cpp (vanilla file)
// [OzMod] Map rotation
cvar_t *om_map_shuffle_once;

// Later...
// [OzMod] Skip re-shuffle if om_map_shuffle_once is enabled
if (g_map_list_shuffle->integer && !om_map_shuffle_once->integer)
{
    // Existing vanilla shuffle code
}
```

**Why it's good:**
- Only adds a cvar declaration and one conditional check
- Doesn't modify vanilla function signatures
- Clear `[OzMod]` markers for easy identification

### ✅ DO: Implement Logic in /ozmod

**Good Example** - Shuffle implementation:
```cpp
// In ozmod/om_main.cpp (OzMod file)
namespace {
    std::vector<std::string> om_str_split(...) {
        // String splitting logic here
    }
}

void Om_ShuffleMapList() {
    auto values = om_str_split(g_map_list->string, ' ');
    std::shuffle(values.begin(), values.end(), mt_rand);
    // ... rest of implementation
}
```

**Why it's good:**
- All complex logic is in OzMod code
- Self-contained helper functions
- Uses vanilla infrastructure without modifying it

### ✅ DO: Use Vanilla Infrastructure

**Good Example** - Reusing vanilla resources:
```cpp
extern std::mt19937 mt_rand;  // Vanilla's RNG
gi.cvar_set("g_map_list", ...);  // Vanilla's cvar system
join_strings(values, " ");  // Vanilla's utility from q_std.h
```

**Why it's good:**
- No duplication of functionality
- Consistent behavior with vanilla code
- Minimal additional code

### ❌ DON'T: Add Complex Logic to Vanilla Files

**Bad Example:**
```cpp
// In g_main.cpp (vanilla file)
void ShuffleMapListOnce() {
    // 50 lines of shuffle implementation
    // This should be in ozmod/
}
```

**Why it's bad:**
- Pollutes vanilla code with OzMod-specific logic
- Makes vanilla code harder to maintain
- Increases merge conflict potential

### ❌ DON'T: Modify Vanilla Function Signatures

**Bad Example:**
```cpp
// In g_main.cpp
void EndDMLevel(bool ozmod_skip_shuffle)  // BAD!
{
    // ...
}
```

**Why it's bad:**
- Breaking change to vanilla interface
- Requires changes throughout the codebase
- Makes vanilla updates difficult

### ❌ DON'T: Create Duplicate Functionality

**Bad Example:**
```cpp
// In ozmod/om_main.cpp
namespace {
    // Reimplementing what's already in q_std.h
    std::string join_strings(...) { }  // BAD!
}
```

**Why it's bad:**
- Code duplication
- Potential behavior inconsistencies
- Larger codebase to maintain

## Code Organization

### Directory Structure
```
ozmod/src/
├── g_*.cpp          # Vanilla files (minimal OzMod changes)
├── p_*.cpp          # Vanilla files (minimal OzMod changes)
├── m_*.cpp          # Vanilla files (minimal OzMod changes)
├── ozmod/           # OzMod-specific code
│   ├── ozmod.h      # OzMod API declarations
│   ├── om_main.cpp  # Core OzMod functionality
│   ├── om_hud.cpp   # HUD system
│   ├── om_menu.cpp  # Menu system
│   └── om_*.cpp     # Other OzMod features
```

### Marking Changes
Always mark vanilla code changes:
```cpp
// [OzMod] Brief description of what and why
code here

// [OzMod] Feature name - specific detail
if (om_feature_enabled->integer) {
    Om_DoSomething();
}
```

## Real-World Example: om_map_shuffle_once

### Vanilla Changes (g_main.cpp)
1. **3 lines** - Cvar declaration and initialization
2. **1 condition** - Check before vanilla shuffle
3. **3 lines** - Call Om_ShuffleMapList() on init

**Total: ~7 lines of vanilla code modified**

### OzMod Implementation (ozmod/om_main.cpp)
1. **12 lines** - Local string split helper
2. **10 lines** - Om_ShuffleMapList() implementation
3. Full debug output and logging

**Total: ~22 lines of OzMod code**

### Result
- **Vanilla code remains clean and maintainable**
- **OzMod feature is self-contained**
- **Easy to disable or remove**
- **No duplicate functionality**

## Checklist for New Features

Before adding a new OzMod feature, ask:

- [ ] Can this be implemented mostly in `/ozmod`?
- [ ] Are vanilla changes clearly marked with `[OzMod]`?
- [ ] Do vanilla changes only add cvars and minimal hooks?
- [ ] Am I reusing vanilla infrastructure instead of duplicating?
- [ ] Can this feature be disabled via a cvar?
- [ ] Will vanilla updates be easy to merge?
- [ ] Is the feature documented?

## Anti-Patterns to Avoid

### 🚫 The "Swiss Army Vanilla"
Adding lots of OzMod logic directly to vanilla files because "it's easier."

### 🚫 The "Duplicate Helper"
Copying utility functions instead of using existing ones from `q_std.h` or vanilla code.

### 🚫 The "Silent Invader"
Making changes without clear `[OzMod]` markers.

### 🚫 The "Signature Breaker"
Modifying vanilla function signatures to pass OzMod-specific parameters.

### 🚫 The "Mandatory Feature"
Creating features that can't be disabled or removed without breaking the build.

## Conclusion

The "thin vanilla" principle keeps OzMod:
- **Maintainable** - Easy to update and debug
- **Respectful** - Minimal impact on original code
- **Modular** - Features can be added/removed independently
- **Transparent** - Clear what's vanilla vs OzMod

When in doubt, ask: "Can this be moved to `/ozmod`?"

