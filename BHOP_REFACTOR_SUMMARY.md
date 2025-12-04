# Bhop Trainer - Thin Vanilla Refactor + Chain Detection Fix

## ✅ All Changes Successfully Applied

### Build Status
```
Build succeeded.
    0 Warning(s)
    0 Error(s)

✅ DLL created and copied successfully!
```

---

## 🎯 What Was Done

### 1. ✅ **Moved Bhop Logic to Trainer Module**
Following the "thin vanilla" architecture principles from `THIN_VANILLA_PRINCIPLES.md`

**Before**: 49 lines of complex logic in `p_client.cpp` (vanilla file)  
**After**: 3 lines minimal hook in vanilla, all logic in `trainer/`

---

## 📝 Changes Applied

### 1. ✅ **Added Chain Tracking Field** (`g_local.h`)
**Line**: 2909

```cpp
// Bhop trainer tracking (per-client)
int32_t bhop_grounded_frames_since_landing;
bool    bhop_recently_landed;
bool    bhop_jump_held_on_landing;
bool    bhop_chain_active; // True when in an active bhop chain (excludes first jump)
uint8_t bhop_result_window_index;
uint8_t bhop_result_window_count;
bool    bhop_result_window[20];
```

---

### 2. ✅ **Added API Declaration** (`trainer/trainer.h`)
**Lines**: 12, 131-132

```cpp
// Forward declarations
struct pmove_t;

// Jump Trainer API
void MapTrainer_UpdateBhopTracking(edict_t *player, pmove_t &pm);
```

---

### 3. ✅ **Implemented in Trainer Module** (`trainer/trainer_jump.cpp`)
**Lines**: 78-149 (71 lines of implementation)

```cpp
void MapTrainer_UpdateBhopTracking(edict_t *player, pmove_t &pm)
{
    if (!player || !player->client)
        return;
    
    // Detect landing transition this frame
    bool was_airborne = !(player->client->ps.pmove.pm_flags & PMF_ON_GROUND);
    bool now_grounded = (pm.s.pm_flags & PMF_ON_GROUND) != 0;
    
    if (was_airborne && now_grounded)
    {
        // Player just landed
        player->client->bhop_recently_landed = true;
        player->client->bhop_grounded_frames_since_landing = 0;
        player->client->bhop_jump_held_on_landing = !!(pm.cmd.buttons & BUTTON_JUMP) || !!(player->client->ps.pmove.pm_flags & PMF_JUMP_HELD);
    }

    // Count grounded frames after landing
    if (player->client->bhop_recently_landed && now_grounded)
        player->client->bhop_grounded_frames_since_landing++;

    // End bhop chain if grounded too long (30 frames ≈ 1 second at 30fps, 0.5s at 60fps)
    if (now_grounded && player->client->bhop_grounded_frames_since_landing > 30)
    {
        player->client->bhop_chain_active = false;
    }

    // Classify when a jump actually happens
    if (pm.jump_sound && !(pm.s.pm_flags & PMF_ON_LADDER))
    {
        bool perfect = player->client->bhop_recently_landed && player->client->bhop_grounded_frames_since_landing <= 1;
        bool late = player->client->bhop_recently_landed && player->client->bhop_grounded_frames_since_landing > 1;
        bool early_or_held = player->client->bhop_recently_landed && player->client->bhop_jump_held_on_landing && player->client->bhop_grounded_frames_since_landing <= 1;

        // Only track and show feedback if we're in an active bhop chain (not the first jump)
        if (player->client->bhop_chain_active)
        {
            // Update rolling window (20 recent jumps)
            const uint8_t window_size = (uint8_t) q_countof(player->client->bhop_result_window);
            if (player->client->bhop_result_window_count < window_size)
                player->client->bhop_result_window_count++;
            player->client->bhop_result_window[player->client->bhop_result_window_index] = perfect;
            player->client->bhop_result_window_index = (player->client->bhop_result_window_index + 1) % window_size;

            // Simple feedback
            if (perfect)
            {
                gi.LocClient_Print(player, PRINT_HIGH, "Bhop: Perfect");
                // Audible feedback for frame-perfect
                gi.sound(player, CHAN_AUTO, gi.soundindex("misc/menu3.wav"), 1.0f, ATTN_NONE, 0);
            }
            else if (early_or_held)
                gi.LocClient_Print(player, PRINT_HIGH, "Bhop: Early/Held");
            else if (late)
                gi.LocClient_Print(player, PRINT_HIGH, G_Fmt("Bhop: Late ({}f)", player->client->bhop_grounded_frames_since_landing).data());
        }
        else
        {
            // First jump - start the chain but don't count it
            player->client->bhop_chain_active = true;
        }

        // Reset landing state after jump
        player->client->bhop_recently_landed = false;
        player->client->bhop_grounded_frames_since_landing = 0;
        player->client->bhop_jump_held_on_landing = false;
    }
}
```

**Features Added**:
- ✅ Chain detection (first jump doesn't count)
- ✅ Chain timeout (30 frames of standing resets chain)
- ✅ Proper bhop sequence tracking
- ✅ Feedback only for actual bhops, not initial jumps

---

### 4. ✅ **Replaced Vanilla Code with Minimal Hook** (`p_client.cpp`)
**Lines**: 3289-3291

**Before** (49 lines):
```cpp
// Bhop consistency detection (server-side only, optional)
if (level.map_trainer.bhop_enabled && ent->client)
{
    // Detect landing transition this frame
    bool was_airborne = !(ent->client->ps.pmove.pm_flags & PMF_ON_GROUND);
    bool now_grounded = (pm.s.pm_flags & PMF_ON_GROUND) != 0;
    if (was_airborne && now_grounded)
    {
        ent->client->bhop_recently_landed = true;
        ent->client->bhop_grounded_frames_since_landing = 0;
        ent->client->bhop_jump_held_on_landing = !!(pm.cmd.buttons & BUTTON_JUMP) || !!(ent->client->ps.pmove.pm_flags & PMF_JUMP_HELD);
    }

    // Count grounded frames after landing
    if (ent->client->bhop_recently_landed && now_grounded)
        ent->client->bhop_grounded_frames_since_landing++;

    // Classify when a jump actually happens
    if (pm.jump_sound && !(pm.s.pm_flags & PMF_ON_LADDER))
    {
        // ... 30 more lines of logic ...
    }
}
```

**After** (3 lines):
```cpp
// [Map Trainer] Bhop consistency tracking
if (level.map_trainer.bhop_enabled)
    MapTrainer_UpdateBhopTracking(ent, pm);
```

**Reduction**: 49 lines → 3 lines (94% reduction!)

---

## 🎯 Thin Vanilla Compliance

| Principle | Before | After | ✓ |
|-----------|--------|-------|---|
| **Minimal vanilla changes** | ❌ 49 lines in vanilla | ✅ 3 lines hook | ✅ |
| **Logic in trainer/** | ❌ All in p_client.cpp | ✅ All in trainer/ | ✅ |
| **Clear [Map Trainer] markers** | ⚠️ Generic comment | ✅ Clear marker | ✅ |
| **No signature changes** | ✅ No changes | ✅ No changes | ✅ |
| **Easy to maintain** | ❌ Hard | ✅ Easy | ✅ |
| **Easy to remove** | ❌ Surgical extraction | ✅ Delete 3 lines | ✅ |

---

## 🐛 Bug Fix: Bhop Chain Detection

### Problem
The old code counted **every jump**, including the first jump from standing/walking. This resulted in confusing feedback like "Bhop: Perfect" when the player only jumped once.

### Solution
Added `bhop_chain_active` tracking:
1. **First jump** → Sets `bhop_chain_active = true`, but **no feedback**
2. **Second jump** (after landing) → Shows feedback (first actual bhop)
3. **Subsequent jumps** → Continue showing feedback
4. **Grounded > 30 frames** → Chain ends, resets to step 1

### Behavior Comparison

| Action | Before | After |
|--------|--------|-------|
| Jump from standing | ✅ "Bhop: Perfect" | ❌ No message (chain starts) |
| 2nd jump (land + jump) | ✅ Counted | ✅ "Bhop: Perfect" |
| 3rd+ jumps | ✅ Counted | ✅ Counted |
| Walk for 1 second | Chain continues | Chain ends |
| Next jump after walking | ✅ Counted | ❌ No message (new chain) |

---

## 📊 Code Organization

### Files Modified

1. **`g_local.h`** (1 line added)
   - Added `bhop_chain_active` field to `gclient_t`

2. **`trainer/trainer.h`** (3 lines added)
   - Added `pmove_t` forward declaration
   - Added `MapTrainer_UpdateBhopTracking()` declaration

3. **`trainer/trainer_jump.cpp`** (+71 lines)
   - Implemented full bhop tracking logic
   - Includes chain detection fix

4. **`p_client.cpp`** (-46 lines)
   - Removed 49 lines of complex logic
   - Replaced with 3-line hook

**Net Result**: 
- Vanilla file: -46 lines
- Trainer module: +74 lines
- Total: +28 lines (for new chain detection feature)

---

## 🎮 User Experience Improvements

### Before (Confusing)
```
Player: *jumps once from standing*
Game: "Bhop: Perfect"
Player: "Wait, I didn't bhop..."
```

### After (Clear)
```
Player: *jumps once from standing*
Game: *silence*

Player: *lands and jumps again*
Game: "Bhop: Perfect"
Player: "Yes! That was a bhop!"

Player: *walks around for a bit*
Game: *chain reset*

Player: *jumps again*
Game: *silence* (new chain starting)
```

---

## ✅ Verification

### Build Status
- ✅ **0 Warnings**
- ✅ **0 Errors**
- ✅ **DLL created successfully**
- ✅ **Copied to Quake 2 directory**

### Code Quality
- ✅ Follows thin vanilla principles
- ✅ All trainer logic isolated in `trainer/`
- ✅ Minimal vanilla file impact
- ✅ Clear separation of concerns
- ✅ Easy to maintain and extend
- ✅ Easy to disable/remove

---

## 📈 Architecture Benefits

### Maintainability
- **Before**: Bhop logic scattered in vanilla file
- **After**: Self-contained in `trainer_jump.cpp`

### Debuggability
- **Before**: Mixed trainer/vanilla concerns
- **After**: Clear separation, easy to debug

### Extensibility
- **Before**: Hard to add features (modify vanilla)
- **After**: Easy to extend in trainer module

### Removability
- **Before**: Surgical extraction from vanilla
- **After**: Delete 3-line hook, done

---

## 🎯 Summary

**Two major improvements in one refactor**:

1. ✅ **Architectural Fix**: Moved bhop logic to trainer module (thin vanilla compliance)
2. ✅ **Functional Fix**: Added chain detection (no more false "bhop" on first jump)

**Result**:
- Cleaner vanilla code (-94% lines in `p_client.cpp`)
- Better user experience (chain detection)
- Easier to maintain (isolated in trainer/)
- Follows established principles (thin vanilla)

**The map trainer now fully adheres to thin vanilla architecture! 🎉**

---

Date: 2025-11-01  
Refactored: `p_client.cpp`, `g_local.h`, `trainer/trainer.h`, `trainer/trainer_jump.cpp`

