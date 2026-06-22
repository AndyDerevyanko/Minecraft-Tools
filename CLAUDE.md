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
**Output:** `<stem>_buildings.world` + `<stem>_buildings.blockIds`
(run on the treeRemover output, so in practice `<base>_trees_buildings.*`)

**Stdin (3 lines, fed by run_tests.py):** world file, blockIds file, dimension
(`overworld`/`nether`/`end`). The dimension is read for input-compatibility; it
does not change classification. No blockProperties file is needed (no axis logic).

### What It Does

Reads the post-treeRemover world, finds player-built structures, and rewrites every
building block as a synthetic `building_marker`. Each building gets a component id,
stored in the **prop field** of its markers — renderer.py colours by that prop.

### Natural vs Unnatural Classification (`naturalByName`)

The search hinges on a per-block-type natural/unnatural split. **Unnatural = building.**

- **Natural:** terrain (dirt/stone/deepslate/sand/gravel/clay/ores via `_ore`),
  fluids, vegetation (grass, flowers, crops, vines, mushrooms, saplings via
  `_sapling`), coral, ice/snow, nether & end terrain, amethyst, raw iron/copper
  ore-vein blocks, and treeRemover's own `tree_marker` / `foliage_marker`.
- **Unnatural (building):** everything else — planks, stairs, slabs, fences, walls,
  glass, wool, carpet, bricks, cobblestone, quartz, furnaces/chests/etc.
- **Wood / logs / leaves are UNNATURAL** (trees were already removed, so leftovers
  are player-placed material). Exceptions kept natural: `azalea_leaves` /
  `flowering_azalea_leaves` (azalea isn't a treeRemover variety, so it survives).
- **Crops are natural** (`wheat`, `carrots`, `potatoes`, `beetroots`, pumpkin/melon
  stems, …) so a tilled field doesn't 8-connect into one giant "building".
- **Path blocks (`dirt_path`, `grass_path`) are UNNATURAL** (player-made).
- **Sandstone & terracotta variants are UNNATURAL** (so builds made of them are
  caught) — but see the discard rule below, which removes natural terrain made of
  them.

### Algorithm

**Phase 1 — Top-down search (per building):**

Blocks are processed topmost-first so a connected structure is fully claimed by the
search starting at its highest unnatural block. For each unclaimed unnatural seed,
walk down layer-by-layer with an `active` candidate set (initially just the seed):
1. If the current (descended) layer holds **no unnatural block**, it is **fully
   natural → discard that layer and stop** the search.
2. Otherwise pull the **whole layer (natural AND unnatural) into the building**,
   then grow it across the plane with an **8-connected BFS through unnatural blocks
   only** (naturals are absorbed but never propagate the BFS).
3. **Descend:** collect the **3×3 footprint below** every block of this layer
   (naturals included), deduped via a set so each lower cell is examined once.

Net effect: an interior dirt floor (a layer that still has walls) is absorbed; the
all-natural ground layer under the lowest wall is the stop condition and is dropped,
so flat terrain isn't swallowed.

**Phase 2 — Discard pure terrain:**

A component whose **unnatural blocks are all a single discardable type**
(`discardableByName`: one kind of sandstone or terracotta — full blocks only, not
stairs/slabs/walls, and not glazed terracotta) is assumed to be natural
desert/badlands terrain, not a build, and is unclaimed. Absorbed natural blocks do
not count toward type-uniformity.

A **separate, independent** rule then discards any component with **5 or fewer
total blocks (natural + unnatural) where every single block is the exact same
type** — almost certainly a stray noise cluster, not a real build (a lone
unclaimed block, size 1, is the most common case). This applies to *any* block
type, not just sandstone/terracotta, and naturals count toward both the size and
the uniformity check here (unlike the sandstone/terracotta rule above).

**Phase 3 — Merge (6-connectivity, U/D/L/R/F/B, no diagonals):**

Union-find over components:
- Two components that touch **face-to-face** are merged.
- If a single **natural block (or one natural layer)** is sandwiched in a straight
  line between two components, they are merged **and that sandwiched block becomes a
  building block**.

**Phase 4 — Output:**

- Surviving components get contiguous colour ids (discarded/empty ones skipped).
- Every building block (incl. absorbed naturals and sandwiched naturals) →
  `building_marker`, with the colour id written into the **prop** field.
- All other blocks → written through unchanged.
- `id/prop` byte width is widened if the new marker id or the largest colour id no
  longer fits the original width.
- `building_marker` is appended to the output `.blockIds` as `maxBlockID+1`.

---


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
