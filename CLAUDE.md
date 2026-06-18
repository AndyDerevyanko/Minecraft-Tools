# Minecraft Tools — Project Context for Claude

## What This Project Is

A pipeline that takes a raw Minecraft world export and produces a colored 3D mesh (.obj) identifying:
- **Trees** (removed/replaced with markers by treeRemover)
- **Player-built structures** (detected by buildingCatcher, rendered as colored components)

The pipeline runs in order:
```
worldExtract.py  →  treeRemover.exe  →  buildingCatcher.exe  →  renderer.py
```

Test runner: `run_tests.py` — compiles both C++ tools, runs them, opens the .obj, asks user for pass/fail verdict. **The user runs tests manually. Never run tests yourself.**

---

## Binary World File Format (`.world`)

Produced by `worldExtract.py` / `worldExtractor.py`. Gzip-compressed.

**Header** (text lines, newline-terminated, inside the gzip stream):
```
[namespace bytes: N]      ← only present if custom/modded world
id/prop bytes: N
x coord bytes: N
y coord bytes: N
z coord bytes: N
```

**Body** (raw binary, little-endian, immediately after header):
Each block record is:
```
[namespace_bytes]  type_id(N bytes)  prop_id(N bytes)  x(N bytes)  y(N bytes)  z(N bytes)
```

Coordinates are sign-magnitude: the MSB is the sign bit, remaining bits are the magnitude.
```cpp
uint64_t signBit = 1ULL << (numBytes*8 - 1);
if (raw & signBit) return -(int)(raw & ~signBit);
return (int)raw;
```

**Companion files:**
- `.blockIds` — `numericID=blockName` one per line
- `.blockProperties` — `propID=key=val,...` one per line (used for log axis detection)

Output world files keep the same format and byte widths. Marker blocks (synthetic types) are appended to `.blockIds`.

---

## Compiler / Build

- **Compiler:** `C:\msys64\mingw64\bin\g++.exe` (or ucrt64)
- **Flags:** `-std=c++20 -O2 -lz`
- **C++20 note:** `ends_with()` OK. `contains()` is C++23 — use `.find() != npos`.

---

## treeRemover.cpp

**Location:** `c++/treeRemover.cpp`  
**Output:** `<stem>_trees.world` + `<stem>_trees.blockIds`

### What It Does

Reads the world file, identifies natural trees, removes their log/wood/leaf blocks from the output, and writes two synthetic marker blocks at the stump location:
- `tree_marker` — confirmed tree stump position
- `foliage_marker` — leaf-only clusters (unconfirmed foliage)

### Key Data Structures

- **`Object`** — linked list of `Layer`s (one Layer per Y level), each Layer holds a `vector<block>`. Has `merge()` and `separate()` methods.
- **`Tree : Object`** — adds `variety` (oak/spruce/etc.), `treeID`, counters.
- **`Sub_Tree : Tree`** — used during orphan checking; has its own instance counter reset per head tree.
- **`FillerLayer : Layer`** — creates a chain of empty Layers for the `merge()` gap-fill case.

### Block Category Sets (global)

```cpp
std::unordered_set<int> woodTypes, logTypes, leafTypes;
std::unordered_map<int, int> blockVariety;   // type → variety index (oak=0..warped=10)
std::unordered_map<int, int> propAxis;        // propID → X_/Y_/Z_
```

Tree varieties: oak, spruce, birch, jungle, acacia, dark_oak, mangrove, cherry, pale_oak, crimson, warped.

`stumpableBlocks`: dirt, grass_block, coarse_dirt, podzol, rooted_dirt, moss_block, pale_moss_block, mud, muddy_mangrove_roots, mycelium, farmland, clay, crimson_nylium, warped_nylium, gravel, sand, red_sand, dirt_path, grass_path, soul_sand, snow_block.

### Algorithm (top-down pass)

**Phase 1 — Scan (top-down, layer by layer):**

For each Y level, from world top to bottom:
- **Section A** (if not first layer): check each block from the PREVIOUS layer's ID map. For each block with a tree ID, look at orthogonal neighbors (not diagonals) on the CURRENT layer. Add matching same-variety tree blocks to that tree. Rules:
  - Only propagate downward from **Y-oriented logs** or **leaves** (horizontal logs do NOT cascade their ID down — prevents building roof beams absorbing pillars).
  - Stumpable blocks are only absorbed if they are DIRECTLY below (`dx==0, dz==0`) a Y-oriented wood/log.
- **Section B** (all layers): any remaining unvisited tree blocks start new `Tree` objects (BFS via `createToTree` / `addSurroundingToTree`).

**Phase 2 — DSU Merge:**

After the scan, trees that share adjacent blocks at any Y level (same variety) are merged via union-find (DSU). This handles multi-trunk trees.

**Phase 3 — Filter (serial Phase A + parallel Phase B):**

Phase A (serial): classify each tree:
- **Keep** (tStatus=0): has a valid stump (stumpable block below a Y-oriented log at the lowest log Y level) AND has leaves.
- **Foliage** (tStatus=1): no valid stump but has leaves → goes to `foilage` vector, written as `foliage_marker`.
- **Discard** (tStatus=2): no stump and no leaves (bare log cluster, a building beam).

Phase B (parallel, all hardware threads):
1. **Log-connectivity BFS** — BFS from Y-oriented logs through adjacent logs. Horizontal logs can only reach neighbors on the same plane (no vertical step). Disconnected logs removed from tree.
2. **L-shape check** — if a horizontal log forms an L-shape (checked with `isUprightL`, `isBackwardsUprightL`, `isUpsideDownL`, `isBackwardsUpsideDownL`) and is NOT part of a square, it's flagged for vertical deletion.
3. **Axis check** — horizontal X/Z logs must have a Y-axis or Z/X-axis neighbor to be valid; otherwise marked `prop=-2` and removed.
4. **Stumpable removal** — stumpable blocks removed from final tree (they were used for stump detection only).

**Phase 4 — Orphan check:**

For each kept tree, decompose into `Sub_Tree` components using the same top-down scan logic. Sub-trees are valid if they have BOTH logs and leaves. Sub-trees that are wood-only (detached horizontal logs) are discarded. Valid sub-trees that were fragmented are added back as separate trees.

**Phase 5 — Output:**

Write the world file with tree/leaf blocks replaced:
- Each tree's stump location → `tree_marker` block written at the stump Y
- Each foliage cluster → `foliage_marker` block
- All other blocks (non-tree) → written through unchanged

---

## buildingCatcher.cpp

**Location:** `c++/buildingCatcher/buildingCatcher.cpp`  
**Input:** `<stem>_trees.world` (treeRemover output)  
**Output:** `<stem>_trees_buildings.world` + `.blockIds`


### Key Types

```cpp
using XZ    = std::pair<int,int>;
using XZSet = std::unordered_set<XZ, XZHash>;
using XZMap = std::unordered_map<XZ, block, XZHash>;
```

`packXYZ(x,y,z)` → `int64_t` key used in `globalVisited` / `buildingCoords`.

### Natural Block Classification (`isNatural`)

Dimension-gated. Three sets:
- `kNaturalSet` — universal (stone, dirt, grass, ores via `ends_with("_ore")`, water, leaves via `ends_with("_leaves")`, etc.)
- `kNetherNaturalSet` — netherrack, soul sand, glowstone, etc. (only natural in NETHER)
- `kEndNaturalSet` — end_stone, chorus_flower, chorus_plant (only natural in END)

Also: `ends_with("_terracotta")` without "glazed" → natural (badlands terrain), `find("_coral") != npos` → natural.

Anything NOT natural → **unnatural** → candidate for building detection.

`isWoodOrLog()` — logs/wood/hyphae/stems (unnatural but used for the all-wood filter).  
`isMarker()` — tree_marker, foliage_marker, building_marker: always skipped.


## renderer.py

Reads the `_trees_buildings.world` file (same binary format). Produces `.obj` + `.mtl`.

**Building component coloring:**
1. Collect all `building_marker` positions.
2. Flood-fill into connected components via **6-connectivity** (face-adjacent in 3D).
3. Each component gets a random HSV hue in range `[0.167 (yellow), 0.667 (blue)]`, sat=0.90, val=0.95.
4. Material names: `building_0`, `building_1`, ...

**Other materials:**
- `tree_marker` → purple (0.50, 0.00, 0.80)
- `foliage_marker` → red
- `leaf` → green
- `wood` → brown
- `stumpable` → dark grey
- `other` → light grey

Mesh: culled faces (only exposed faces rendered). Quads written as `f` entries.

---

## run_tests.py

- Reads `*.test` files (3 lines: stem, y/n custom, dimension).
- Compiles both C++ tools fresh each run.
- Runs the full pipeline on each test world.
- Opens the resulting `.obj` in the system viewer.
- Asks the user for pass/fail verdict.
- Saves results to `test_results.txt`.

**IMPORTANT: The user runs tests. Claude must never invoke run_tests.py or any pipeline step.**

---

## Important Rules (from user)

- Keep all debug `std::cout` statements in buildingCatcher.cpp — do not remove them.
- Do not run tests; user runs them and verifies visually.
- Do not add comments explaining WHAT code does; only add a comment when the WHY is non-obvious.
- `contains()` is C++23 — always use `.find() != std::string::npos`.

---
