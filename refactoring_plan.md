# Panda Refactoring Plan - Source + Header File Separation
## Issue #2171 Bounty ($500 USD)

---

## Current State Analysis

### Problem:
All C/C++ code is currently in `.h` header files. This creates several issues:
- Large monolithic headers (63,975+ lines total)
- Compilation unit bloat
- Difficult to maintain and add new drivers
- No separation of interface vs implementation

### Example file structure (current):
```
board/
├── main_comms.h     // Contains both declarations AND implementations (~600 lines)
├── can.h            // Simple interface-only header (~5 lines)
├── uart.h           // Implementation mixed with interface (~80 lines)
├── pwm.h            // Implementation mixed with interface (~200 lines)
└── ...
```

---

## Target Structure (After Refactor)

### Goal:
Separate into `.h` (declarations) + `.c`/`.cpp` (implementations)

```
board/
├── main_comms.h       ← Only function declarations
├── main_comms.c       ← Function implementations
├── can.h              ← Keep as-is (interface-only)
├── uart.h             ← Interface declaration
├── uart.c             ← Implementation
├── pwm.h              ← Interface declaration
├── pwm.c              ← Implementation
└── ...
```

---

## Implementation Strategy

### Phase 1: Small Driver Files First (Low Risk) ⭐

**Why start small?**
- Low risk of breaking build
- Easy to test incrementally
- Build confidence quickly

**Priority files (sorted by size):**

| File | Lines | Risk Level | Notes |
|------|-------|------------|-------|
| `fake_stm.h` | ~300 | ⭐⭐ Low | Simple simulation layer |
| `drivers/spi.h` | ~150 | ⭐⭐ Low | SPI driver interface |
| `drivers/timers.h` | ~200 | ⭐⭐ Low | Timer functions |
| `drivers/pwm.h` | ~200 | ⭐⭐ Low | PWM control |
| `drivers/led.h` | ~100 | ⭐ Very Low | LED control |

**For each file:**
1. Create corresponding `.c` file with same basename
2. Move function implementations from `.h` to `.c`
3. Keep only `inline` or `static inline` declarations in `.h`
4. Update SCons build to include new `.c` files

---

### Phase 2: Medium Complexity Drivers

**Next files to refactor:**

| File | Lines | Priority |
|------|-------|----------|
| `drivers/fdcan.h` | ~300 | High |
| `drivers/uart.h` | ~150 | High |
| `body/can.h` | ~200 | Medium |
| `drivers/harness.h` | ~100 | Low |

---

### Phase 3: Complex Files

**Remaining large files:**

| File | Lines | Notes |
|------|-------|-------|
| `main_comms.h` | ~600 | **High priority** - core comms layer |
| `libc.h` | ~500 | Standard library wrapper |
| `can_common.h` | ~300 | Shared CAN functionality |

---

## Technical Details

### For Each File:

**Step 1: Create `.c` file**
```bash
# From: board/drivers/pwm.h
# To: 
#   board/drivers/pwm.h (declarations only)
#   board/drivers/pwm.c (implementations)
```

**Step 2: Refactor header** (`pwm.h`)
```c
// BEFORE (in header):
void pwm_set_duty_cycle(int channel, float duty);  // Declaration
void pwm_set_duty_cycle(int channel, float duty) {  // Implementation here too!
    // ... actual code ...
}

// AFTER:
// In pwm.h:
void pwm_set_duty_cycle(int channel, float duty);  // Only declaration!

// In pwm.c:
#include "pwm.h"
void pwm_set_duty_cycle(int channel, float duty) {  // Implementation moved here
    // ... actual code ...
}
```

**Step 3: Update SCons build**
```python
# In SConscript files:
env.CCompiler('pwm.c')  # Add to build
```

---

## Testing Checklist

For each refactored file:

- [ ] Compiles without errors
- [ ] All existing tests pass
- [ ] No change in compiled binary size
- [ ] Runtime behavior identical

---

## PR Guidelines

Per issue requirements: **"clean PR with minimal changes"**

### Do:
- ✅ Incremental commits per driver
- ✅ Keep scope minimal per commit
- ✅ Document changes in commit messages
- ✅ Test before pushing

### Don't:
- ❌ Refactor all files at once (too risky)
- ❌ Change logic while moving code
- ❌ Modify unrelated code in the same PR

---

## Timeline Estimate

| Phase | Estimated Time |
|-------|----------------|
| Phase 1 (small drivers) | 2-3 hours |
| Phase 2 (medium) | 1-2 hours |
| Phase 3 (complex) | 3-4 hours |
| Testing & fixes | 1-2 hours |
| **Total** | **~8-10 hours** |

---

## Success Criteria

To claim bounty ($500):

1. ✅ All refactored code compiles cleanly
2. ✅ All existing tests pass
3. ✅ No functional changes
4. ✅ Code structure is cleaner
5. ✅ Easier to add new drivers (proven by example)

---

## Contact for Questions

If any blockers during refactoring, comment on Issue #2171.
