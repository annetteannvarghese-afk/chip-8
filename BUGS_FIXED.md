# BUGS FIXED — CHIP-8 Emulator (TatHack'26)

All bugs were found by reading the source code carefully and confirmed by running standard CHIP-8 ROMs.

---

## Bug #1 — Wrong Stack Pop Order (Return from Subroutine)

| Field | Detail |
|---|---|
| **File** | `src/chip8.cpp` |
| **Opcode** | `0x00EE` — Return from subroutine |
| **Symptom** | Terminal flooded with `Unknown opcode: 0x0`; game crashed immediately |

### What Was Wrong
```cpp
// BUGGY CODE:
pc = stack[sp];   // reads the EMPTY slot above the saved address
sp--;             // decrements AFTER reading (too late!)
pc += 2;
```

### The Fix
```cpp
// FIXED CODE:
sp--;             // Step 1: move pointer DOWN to where the address was saved
pc = stack[sp];   // Step 2: read the correct return address
pc += 2;          // Step 3: skip past the original CALL instruction
```

---

## Bug #2 — BCD Tens Digit Wrong Formula

| Field | Detail |
|---|---|
| **File** | `src/chip8.cpp` |
| **Opcode** | `0xFX33` — Store BCD representation |
| **Symptom** | Scores and number displays showed garbage values |

### What Was Wrong
```cpp
memory[index+1] = value / 10;    // WRONG: gives 25 for value=253
```

### The Fix
```cpp
memory[index+1] = (value / 10) % 10; // FIXED: gives 5 for value=253
```

---

## Bug #3 — Register Store/Load Off-By-One (FX55 / FX65)

| Field | Detail |
|---|---|
| **File** | `src/chip8.cpp` |
| **Opcodes** | `0xFX55` (store), `0xFX65` (load) |
| **Symptom** | Subtle register corruption — last register always had wrong value |

### What Was Wrong
```cpp
for(int i=0; i < x; i++) // Misses Vx itself!
```

### The Fix
```cpp
for(int i=0; i <= x; i++) // Includes Vx as the spec requires
```

---

## Bug #4 — FX0A Key-Wait Doesn't Actually Block

| Field | Detail |
|---|---|
| **File** | `src/chip8.cpp` |
| **Opcode** | `0xFX0A` — Wait for key press |
| **Symptom** | Games skipped past input-wait screens instantly |

### What Was Wrong
```cpp
pc += 2;  // ALWAYS advanced, even with no key pressed
```

### The Fix
```cpp
if(key_pressed) pc += 2; // Only advances when a key is found
// If no key: instruction re-runs → blocks!
```

---

## Bug #5 — Main Loop Timing: SDL_Delay Inside Cycle Loop

| Field | Detail |
|---|---|
| **File** | `src/main.cpp` |
| **Symptom** | Emulator ran at ~6 FPS instead of 60 FPS |

### What Was Wrong
```cpp
for(int i=0; i<10; i++){
    chip8.emulate_cycle();
    SDL_Delay(16); // 16ms × 10 = 160ms per frame = 6 FPS!
}
```

### The Fix
```cpp
for(int i=0; i<cycles_per_frame; i++){
    chip8.emulate_cycle(); // no delay inside
}
SDL_Delay(16); // One delay per frame = 60 FPS
```

---

## Bug #6 — Graphics Y-Axis Flipped Upside-Down

| Field | Detail |
|---|---|
| **File** | `src/main.cpp` |
| **Function** | `draw_graphics()` |
| **Symptom** | Display mirrored vertically — scores at bottom, paddles at top |

### What Was Wrong
```cpp
SDL_Rect rect = {x*SCALE, (31-y)*SCALE, SCALE, SCALE}; // flips Y!
```

### The Fix
```cpp
SDL_Rect rect = {x*SCALE, y*SCALE, SCALE, SCALE}; // no flip needed
```

---

## Summary Table

| # | File | Area | Bug | Impact |
|---|---|---|---|---|
| 1 | `chip8.cpp` | `0x00EE` | Stack pop wrong order | **Critical** |
| 2 | `chip8.cpp` | `0xFX33` | BCD tens: `value/10` | **Medium** |
| 3 | `chip8.cpp` | `0xFX55/65` | Loop `i < x` | **Medium** |
| 4 | `chip8.cpp` | `0xFX0A` | Always advances PC | **High** |
| 5 | `main.cpp` | Main loop | Delay inside cycle loop | **High** |
| 6 | `main.cpp` | `draw_graphics` | `(31-y)` Y-flip | **Medium** |
