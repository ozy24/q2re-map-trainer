# Bhop Trainer Debug Logging Guide

## ✅ Debug Logging Enabled

The bhop trainer now writes comprehensive debug logs to `trainer.log` in the Quake 2 directory (same location as the game DLL).

---

## 📝 Log Location

**File**: `trainer.log` (in your Quake 2 installation directory)

The log file is created/appended to automatically whenever bhop tracking is enabled.

---

## 🔍 Log Events

### **LANDED** - Player touches ground after being airborne
```
[HH:MM:SS] LANDED: chain_active=0, recently_landed=0->1, frames=0->0
```

**What it means**:
- `chain_active`: Was a bhop chain active? (0=no, 1=yes)
- `recently_landed`: Previous value -> new value (always becomes 1)
- `frames`: Previous frame count -> new value (always becomes 0)

---

### **FRAME_COUNT** - Frame counter incrementing (every 10 frames)
```
[HH:MM:SS] FRAME_COUNT: chain_active=1, frames=10
[HH:MM:SS] FRAME_COUNT: chain_active=1, frames=20
```

**What it means**:
- Player is grounded and chain is active
- Frame counter is incrementing
- Logs every 10 frames to avoid spam

---

### **TIMEOUT** - Chain timed out (30+ frames grounded)
```
[HH:MM:SS] TIMEOUT: chain_active=1->0, frames=31, resetting state
```

**What it means**:
- Chain was active, now ending
- Player stood still for 30+ frames
- All state variables reset

---

### **JUMP** - Player jumped
```
[HH:MM:SS] JUMP: chain_active=0, recently_landed=0, frames=0, perfect=0, late=0
```

**What it means**:
- `chain_active`: Is chain currently active?
- `recently_landed`: Was there a recent landing?
- `frames`: How many frames since landing?
- `perfect`: Would this be a perfect bhop? (<=1 frame)
- `late`: Would this be late? (>1 frame)

**Followed by one of**:
```
[HH:MM:SS]   -> Starting chain (no feedback)
[HH:MM:SS]   -> Feedback: Perfect
[HH:MM:SS]   -> Feedback: Late (5f)
[HH:MM:SS]   -> Feedback: Early/Held
```

---

### **Reset** - State reset after jump
```
[HH:MM:SS]   -> Reset: recently_landed=0, frames=0
```

**What it means**:
- Landing state cleared after processing jump
- Ready for next landing

---

## 🎯 How to Debug the Ghost Timing Issue

### Test Sequence

1. **Start Quake 2** with bhop trainer enabled
2. **Do 2-3 bhops** (should see "Perfect" or "Late (Nf)" feedback)
3. **Stop and stand still** for 2+ seconds
4. **Start bhopping again**:
   - First jump should be SILENT
   - Second jump should show timing
5. **Check `trainer.log`**

### What to Look For

**Expected flow for starting a new chain after timeout:**

```
[HH:MM:SS] TIMEOUT: chain_active=1->0, frames=31, resetting state
... (player standing, no logs) ...
[HH:MM:SS] JUMP: chain_active=0, recently_landed=0, frames=0, perfect=0, late=0
[HH:MM:SS]   -> Starting chain (no feedback)
[HH:MM:SS]   -> Reset: recently_landed=0, frames=0
[HH:MM:SS] LANDED: chain_active=1, recently_landed=0->1, frames=0->0
[HH:MM:SS] JUMP: chain_active=1, recently_landed=1, frames=1, perfect=1, late=0
[HH:MM:SS]   -> Feedback: Perfect
```

**What would indicate the bug:**

```
[HH:MM:SS] TIMEOUT: chain_active=1->0, frames=31, resetting state
... (player standing) ...
[HH:MM:SS] JUMP: chain_active=0, recently_landed=1, frames=31, perfect=0, late=1  ← BUG!
```
If you see `recently_landed=1` or `frames=31` when `chain_active=0`, that's stale data!

---

## 🐛 Known Issues to Check

### Issue 1: Stale landing state when chain inactive
**Symptom**: After timeout, `recently_landed` is still 1
**Look for**: `JUMP: chain_active=0, recently_landed=1` (BAD!)
**Should be**: `JUMP: chain_active=0, recently_landed=0` (GOOD!)

### Issue 2: Frame counter running when chain inactive
**Symptom**: After timeout, `frames` is still incrementing
**Look for**: `FRAME_COUNT` logs appearing when `chain_active=0` (BAD!)
**Should be**: No `FRAME_COUNT` logs when `chain_active=0` (GOOD!)

### Issue 3: Frame counter not reset on timeout
**Symptom**: High frame count persists after timeout
**Look for**: `JUMP: chain_active=0, frames=50` after a timeout (BAD!)
**Should be**: `JUMP: chain_active=0, frames=0` (GOOD!)

---

## 📊 Example Log Session

**Good Session** (working correctly):
```
[15:30:00] JUMP: chain_active=0, recently_landed=0, frames=0, perfect=0, late=0
[15:30:00]   -> Starting chain (no feedback)
[15:30:00]   -> Reset: recently_landed=0, frames=0
[15:30:01] LANDED: chain_active=1, recently_landed=0->1, frames=0->0
[15:30:01] JUMP: chain_active=1, recently_landed=1, frames=1, perfect=1, late=0
[15:30:01]   -> Feedback: Perfect
[15:30:01]   -> Reset: recently_landed=0, frames=0
[15:30:02] LANDED: chain_active=1, recently_landed=0->1, frames=0->0
[15:30:02] FRAME_COUNT: chain_active=1, frames=10
[15:30:02] FRAME_COUNT: chain_active=1, frames=20
[15:30:02] FRAME_COUNT: chain_active=1, frames=30
[15:30:03] TIMEOUT: chain_active=1->0, frames=31, resetting state
... (standing) ...
[15:30:05] JUMP: chain_active=0, recently_landed=0, frames=0, perfect=0, late=0
[15:30:05]   -> Starting chain (no feedback)
```

---

## 🔧 Testing Instructions

1. **Delete old log**: Remove `trainer.log` before testing
2. **Enable bhop trainer**: Use the in-game menu
3. **Execute test sequence**:
   ```
   - Jump once (should be silent)
   - Land and jump again (should show timing)
   - Do 2-3 more bhops
   - Stand still for 2 seconds
   - Jump once (should be silent)
   - Land and jump again (should show timing)
   ```
4. **Check the log**: Look for the patterns described above

---

## 📤 Sharing Logs

When reporting the issue, please share:
1. The entire `trainer.log` file
2. Description of what you did
3. What feedback you saw in-game
4. What you expected vs what happened

This will help identify exactly where the state machine is failing!

---

## 🎯 Quick Check

After testing, search the log for:
- `chain_active=0, recently_landed=1` ← **This is the bug!**
- `chain_active=0` followed immediately by `Feedback:` ← **This is the bug!**
- `TIMEOUT:` followed by `JUMP:` with `frames` > 0 ← **This is the bug!**

If you find any of these patterns, the state isn't being properly reset between chains.

---

Date: 2025-11-01  
File: `trainer_jump.cpp` (lines 9-213)  
Log Location: `<Q2_DIR>/trainer.log`

