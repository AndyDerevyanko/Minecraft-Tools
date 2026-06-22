#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <climits>
#include <algorithm>
#include <numeric>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <zlib.h>

// ── coordinate key for sparse 3D lookups ──────────────────────────────────────
struct Key
{
    int x, y, z;
    bool operator==(const Key &o) const { return x == o.x && y == o.y && z == o.z; }
};

struct KeyHash
{
    size_t operator()(const Key &k) const
    {
        uint64_t h = 1469598103934665603ULL;
        h = (h ^ (uint32_t)k.x) * 1099511628211ULL;
        h = (h ^ (uint32_t)k.y) * 1099511628211ULL;
        h = (h ^ (uint32_t)k.z) * 1099511628211ULL;
        return (size_t)h;
    }
};

// ── binary helpers (identical scheme to treeRemover) ──────────────────────────
uint64_t readLE(const uint8_t *buf, int numBytes)
{
    uint64_t val = 0;
    for (int i = 0; i < numBytes; i++)
        val |= (uint64_t)(unsigned char)buf[i] << (8 * i);
    return val;
}

int decodeCoord(uint64_t raw, int numBytes)
{
    uint64_t signBit = 1ULL << (numBytes * 8 - 1);
    if (raw & signBit)
        return -(int)(raw & ~signBit);
    return (int)raw;
}

static bool endsWith(const std::string &s, const std::string &suffix)
{
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Classify a block name as a naturally-occurring world block. Everything that is
// NOT natural is considered player-placed and therefore part of a building.
// Trees were already stripped by treeRemover, so any leftover wood/log/leaf is
// treated as unnatural here (per the building-detection spec).
static bool naturalByName(const std::string &name)
{
    // azalea bushes are natural foliage, not trees, so treeRemover leaves their
    // leaves behind — keep them natural despite the "_leaves" suffix below
    if (name == "azalea_leaves" || name == "flowering_azalea_leaves")
        return true;

    // leftover tree material is unnatural (treeRemover already removed real trees)
    if (endsWith(name, "_log") || endsWith(name, "_wood") || endsWith(name, "_hyphae") ||
        endsWith(name, "_leaves") || name == "crimson_stem" || name == "warped_stem")
        return false;

    // naturally generated ores and saplings
    if (endsWith(name, "_ore") || endsWith(name, "_sapling"))
        return true;

    // NOTE: sandstone and terracotta variants are intentionally UNNATURAL so that
    // builds made of them are detected. They DO occur as natural desert/badlands
    // terrain, but that is filtered later: any component made purely of one kind of
    // sandstone or terracotta is discarded (see discardableByName / the discard pass).
    // Path blocks (dirt_path / grass_path) are player-made and likewise unnatural.

    // coral reefs (live and dead coral blocks / fans)
    if (endsWith(name, "_coral") || endsWith(name, "_coral_block") ||
        endsWith(name, "_coral_fan") || endsWith(name, "_coral_wall_fan"))
        return true;

    static const std::unordered_set<std::string> natural = {
        // air + liquids (usually absent from the export, kept for safety)
        "air", "cave_air", "void_air", "water", "lava", "bubble_column",
        // treeRemover's own markers represent natural tree features
        "tree_marker", "foliage_marker",
        // dirt family (dirt_path / grass_path are deliberately omitted — unnatural)
        "grass_block", "dirt", "coarse_dirt", "podzol", "rooted_dirt", "mycelium",
        "mud", "muddy_mangrove_roots", "farmland",
        "mangrove_roots", "moss_block", "pale_moss_block", "moss_carpet", "pale_moss_carpet",
        // loose terrain
        "sand", "red_sand", "gravel", "clay", "suspicious_sand", "suspicious_gravel",
        // stone family
        "stone", "granite", "diorite", "andesite", "deepslate", "tuff", "calcite",
        "dripstone_block", "pointed_dripstone", "bedrock", "obsidian", "crying_obsidian",
        "smooth_basalt", "magma_block",
        // raw-metal blocks generate inside 1.18+ ore veins
        "raw_iron_block", "raw_copper_block",
        // amethyst geodes
        "amethyst_block", "budding_amethyst", "amethyst_cluster",
        "small_amethyst_bud", "medium_amethyst_bud", "large_amethyst_bud",
        // nether terrain
        "ancient_debris", "netherrack", "soul_sand", "soul_soil", "glowstone", "basalt",
        "blackstone", "gilded_blackstone", "crimson_nylium", "warped_nylium", "shroomlight",
        "nether_wart_block", "warped_wart_block", "bone_block",
        // end terrain
        "end_stone", "chorus_flower", "chorus_plant",
        // ice / snow
        "snow", "snow_block", "powder_snow", "ice", "packed_ice", "blue_ice", "frosted_ice",
        // misc natural
        "cobweb", "turtle_egg", "frogspawn", "sea_pickle", "bee_nest", "pumpkin",
        "sculk", "sculk_vein", "sculk_sensor", "sculk_catalyst", "sculk_shrieker",
        // grasses / small flora
        "short_grass", "grass", "tall_grass", "fern", "large_fern", "dead_bush", "bush",
        "firefly_bush", "leaf_litter", "short_dry_grass", "tall_dry_grass", "wildflowers",
        // vines / climbers
        "vine", "weeping_vines", "weeping_vines_plant", "twisting_vines", "twisting_vines_plant",
        "cave_vines", "cave_vines_plant", "glow_lichen", "hanging_roots", "spore_blossom",
        "big_dripleaf", "big_dripleaf_stem", "small_dripleaf", "pink_petals",
        // water plants
        "kelp", "kelp_plant", "seagrass", "tall_seagrass", "lily_pad",
        // tall plants
        "bamboo", "bamboo_sapling", "sugar_cane", "cactus", "nether_wart", "sweet_berry_bush",
        "azalea", "flowering_azalea",
        // crops (so a tilled field of them is not mistaken for one big building)
        "wheat", "carrots", "potatoes", "beetroots", "pumpkin_stem", "melon_stem",
        "attached_pumpkin_stem", "attached_melon_stem", "torchflower_crop", "cocoa", "fire", "soul_fire",
        // nether/mushroom flora
        "crimson_roots", "warped_roots", "nether_sprouts", "crimson_fungus", "warped_fungus",
        "brown_mushroom", "red_mushroom", "brown_mushroom_block", "red_mushroom_block", "mushroom_stem",
        // flowers
        "dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "red_tulip", "orange_tulip",
        "white_tulip", "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley", "wither_rose",
        "torchflower", "sunflower", "lilac", "rose_bush", "peony", "pitcher_plant", "pitcher_crop",
        "closed_eyeblossom", "open_eyeblossom"};

    return natural.count(name) > 0;
}

// A "discardable" block is a full-block sandstone or terracotta variant — the
// forms that also occur as natural desert/badlands terrain. A whole building
// component built purely from a single discardable type is assumed to be that
// terrain (not a real build) and is thrown away. Stairs/slabs/walls and glazed
// terracotta are excluded: those are unambiguously crafted, never raw terrain.
static bool discardableByName(const std::string &name)
{
    static const std::unordered_set<std::string> sandstone = {
        "sandstone", "red_sandstone",
        "smooth_sandstone", "smooth_red_sandstone",
        "cut_sandstone", "cut_red_sandstone",
        "chiseled_sandstone", "chiseled_red_sandstone"};
    if (sandstone.count(name))
        return true;

    if (name == "terracotta" ||
        (endsWith(name, "_terracotta") && !endsWith(name, "_glazed_terracotta")))
        return true;

    return false;
}

int main()
{
    std::string worldName, worldBlockIDs, dimension;

    std::cout << "Please enter world file name + extension (ex. myworld_trees.world): ";
    std::cin >> worldName;
    std::cout << "Please enter world block ID list + extension (ex. myworld_trees.blockIds): ";
    std::cin >> worldBlockIDs;
    std::cout << "Please enter dimension (overworld/nether/end): ";
    std::cin >> dimension;

    gzFile gz = gzopen(worldName.c_str(), "rb");
    std::ifstream blockIDs(worldBlockIDs);
    if (!gz || blockIDs.fail())
    {
        std::cerr << "Failed to open one or more files.\n";
        if (gz) gzclose(gz);
        return 1;
    }

    auto gzGetLine = [](gzFile f) -> std::string {
        std::string s;
        int c;
        while ((c = gzgetc(f)) != -1 && c != '\n')
            s += (char)c;
        return s;
    };

    // ── block IDs → natural / unnatural classification ────────────────────────
    std::unordered_set<int> naturalIDs;
    std::unordered_set<int> discardableIDs; // sandstone/terracotta terrain forms
    int maxBlockID = 0;
    {
        std::string line;
        while (std::getline(blockIDs, line))
        {
            size_t pos = line.find('=');
            if (pos == std::string::npos) { std::cerr << "error in blockID file" << std::endl; continue; }

            int numericID = 0;
            for (size_t k = 0; k < pos; k++)
                numericID = numericID * 10 + (line[k] - '0');
            if (numericID > maxBlockID) maxBlockID = numericID;

            std::string blockName = line.substr(pos + 1);
            if (naturalByName(blockName))
                naturalIDs.insert(numericID);
            if (discardableByName(blockName))
                discardableIDs.insert(numericID);
        }
    }

    // ── world header (text lines inside the gzip stream) ──────────────────────
    auto parseHeaderLine = [](const std::string &hLine) -> int {
        size_t pos = hLine.rfind(' ');
        int val = 0;
        for (size_t k = (pos == std::string::npos ? 0 : pos + 1); k < hLine.size(); k++)
            if (hLine[k] >= '0' && hLine[k] <= '9')
                val = val * 10 + (hLine[k] - '0');
        return val;
    };

    std::string hLine = gzGetLine(gz);
    bool customWorld = (hLine.find("namespace") != std::string::npos);

    int NS_BYTES = 0, ID_PROP_BYTES, X_BYTES, Y_BYTES, Z_BYTES;
    if (customWorld)
    {
        NS_BYTES = parseHeaderLine(hLine);
        hLine = gzGetLine(gz);
        ID_PROP_BYTES = parseHeaderLine(hLine);
    }
    else
    {
        ID_PROP_BYTES = parseHeaderLine(hLine);
    }
    { std::string tmp = gzGetLine(gz); X_BYTES = parseHeaderLine(tmp); }
    { std::string tmp = gzGetLine(gz); Y_BYTES = parseHeaderLine(tmp); }
    { std::string tmp = gzGetLine(gz); Z_BYTES = parseHeaderLine(tmp); }

    z_off_t binaryStart = gztell(gz);
    int typeOff = customWorld ? NS_BYTES : 0;
    int coordsOff = typeOff + 2 * ID_PROP_BYTES;
    int BLOCK_SIZE = coordsOff + X_BYTES + Y_BYTES + Z_BYTES;

    // ── load every block (natural + unnatural) into a sparse grid ─────────────
    std::vector<Key> pos;        // coordinates per block
    std::vector<char> isNat;     // natural flag per block
    std::vector<int> btype;      // block type id per block (for the discard pass)
    std::vector<int> comp;       // building component per block (-1 = none yet)
    std::unordered_map<Key, int, KeyHash> idx; // coordinate → block index

    int minY = INT_MAX, maxY = INT_MIN;
    {
        std::vector<uint8_t> buf(BLOCK_SIZE);
        while (gzread(gz, buf.data(), BLOCK_SIZE) == BLOCK_SIZE)
        {
            int type = (int)readLE(buf.data() + typeOff, ID_PROP_BYTES);
            int off = coordsOff;
            int x = decodeCoord(readLE(buf.data() + off, X_BYTES), X_BYTES); off += X_BYTES;
            int y = decodeCoord(readLE(buf.data() + off, Y_BYTES), Y_BYTES); off += Y_BYTES;
            int z = decodeCoord(readLE(buf.data() + off, Z_BYTES), Z_BYTES);

            Key k{x, y, z};
            int index = (int)pos.size();
            pos.push_back(k);
            isNat.push_back(naturalIDs.count(type) ? 1 : 0);
            btype.push_back(type);
            comp.push_back(-1);
            idx[k] = index;

            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
        }
    }
    int n = (int)pos.size();

    std::cout << "Loaded " << n << " blocks  (dimension: " << dimension << ")" << std::endl;
    // An empty world (e.g. a tiny test) is not an error — fall through and emit a
    // valid header-only output world so the pipeline keeps going.

    // ── top-down building search ──────────────────────────────────────────────
    // Process blocks topmost-first so each connected structure is claimed in full
    // by the search that starts at its highest unnatural block.
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) { return pos[a].y > pos[b].y; });

    const int dirs8[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

    int compCount = 0;
    for (int oi = 0; oi < n; oi++)
    {
        int start = order[oi];
        if (isNat[start] || comp[start] != -1)
            continue;

        int compId = compCount++;
        int level = pos[start].y;

        // candidate cells on the current layer — initially just the starting block,
        // thereafter the 3x3-below footprint collected from the layer above
        std::vector<Key> active;
        active.push_back(pos[start]);

        while (!active.empty())
        {
            // A fully natural descended layer means the structure has ended: that
            // layer is discarded and the search stops. Any unnatural block keeps it.
            bool hasUnnatural = false;
            for (const Key &k : active)
            {
                auto it = idx.find(k);
                if (it != idx.end() && !isNat[it->second]) { hasUnnatural = true; break; }
            }
            if (!hasUnnatural)
                break;

            // Pull the whole descended layer (natural AND unnatural) into the
            // building, then grow it across this plane through unnatural blocks
            // only (8-connected). Naturals are absorbed but never propagate the BFS.
            std::vector<Key> layer;
            std::vector<Key> stack;
            for (const Key &k : active)
            {
                auto it = idx.find(k);
                if (it == idx.end()) continue;
                int b = it->second;
                if (comp[b] == -1)
                {
                    comp[b] = compId;
                    layer.push_back(k);
                    if (!isNat[b]) stack.push_back(k); // seed for same-plane growth
                }
                else if (comp[b] == compId)
                {
                    layer.push_back(k);
                }
                // claimed by another building → leave it for the merge step
            }

            while (!stack.empty())
            {
                Key c = stack.back();
                stack.pop_back();
                for (auto &d : dirs8)
                {
                    Key nb{c.x + d[0], level, c.z + d[1]};
                    auto i2 = idx.find(nb);
                    if (i2 == idx.end()) continue;
                    int bb = i2->second;
                    if (!isNat[bb] && comp[bb] == -1)
                    {
                        comp[bb] = compId;
                        layer.push_back(nb);
                        stack.push_back(nb);
                    }
                }
            }

            // descend into the 3x3 footprint below every block of this layer.
            // Dedup via a set so each cell below is examined once, not up to 9 times.
            std::unordered_set<Key, KeyHash> nextSet;
            for (const Key &k : layer)
                for (int dx = -1; dx <= 1; dx++)
                    for (int dz = -1; dz <= 1; dz++)
                    {
                        Key bk{k.x + dx, level - 1, k.z + dz};
                        if (idx.find(bk) != idx.end())
                            nextSet.insert(bk);
                    }

            active.assign(nextSet.begin(), nextSet.end());
            level--;
        }
    }

    std::cout << "Search found " << compCount << " raw building components" << std::endl;

    // ── discard pure sandstone / terracotta terrain ───────────────────────────
    // A component whose unnatural blocks are all a single discardable type (one
    // kind of sandstone or terracotta) is natural desert/badlands terrain, not a
    // build — unclaim it. Absorbed natural blocks don't count toward uniformity.
    {
        std::vector<int> uType(compCount, -2);  // -2 = unseen, -3 = mixed
        std::vector<char> hasUnnat(compCount, 0);
        for (int i = 0; i < n; i++)
        {
            if (comp[i] < 0 || isNat[i]) continue;
            int c = comp[i];
            hasUnnat[c] = 1;
            if (uType[c] == -2) uType[c] = btype[i];
            else if (uType[c] != btype[i]) uType[c] = -3;
        }

        std::vector<char> discardComp(compCount, 0);
        int discarded = 0;
        for (int c = 0; c < compCount; c++)
            if (hasUnnat[c] && uType[c] >= 0 && discardableIDs.count(uType[c]))
            {
                discardComp[c] = 1;
                discarded++;
            }

        if (discarded)
            for (int i = 0; i < n; i++)
                if (comp[i] >= 0 && discardComp[comp[i]])
                    comp[i] = -1;

        std::cout << "Discarded " << discarded << " pure sandstone/terracotta components" << std::endl;
    }

    // ── discard tiny single-block-type components (size <= 5) ─────────────────
    // NEW rule, independent of the sandstone/terracotta discard above: any
    // component with 5 or fewer total blocks (natural + unnatural) where every
    // single block is the exact same type is almost certainly noise (a stray
    // leftover cluster), not a real build — unclaim it. A lone unnatural block
    // with nothing absorbed (size 1) is the most common case this catches.
    {
        std::vector<int> tinySize(compCount, 0);
        std::vector<int> tinyType(compCount, -2); // -2 = unseen, -3 = mixed
        for (int i = 0; i < n; i++)
        {
            if (comp[i] < 0) continue;
            int c = comp[i];
            tinySize[c]++;
            if (tinyType[c] == -2) tinyType[c] = btype[i];
            else if (tinyType[c] != btype[i]) tinyType[c] = -3;
        }

        std::vector<char> discardTiny(compCount, 0);
        int discardedTiny = 0;
        for (int c = 0; c < compCount; c++)
            if (tinySize[c] > 0 && tinySize[c] <= 5 && tinyType[c] >= 0)
            {
                discardTiny[c] = 1;
                discardedTiny++;
            }

        if (discardedTiny)
            for (int i = 0; i < n; i++)
                if (comp[i] >= 0 && discardTiny[comp[i]])
                    comp[i] = -1;

        std::cout << "Discarded " << discardedTiny << " tiny single-block-type components (<=5 blocks)" << std::endl;
    }

    // ── merge touching buildings (6-connectivity, no diagonals) ───────────────
    // Direct face contact merges two components. A single natural block (or one
    // natural layer) sandwiched in a straight line between two components also
    // merges them, and that sandwiched block becomes part of the building.
    std::vector<int> par(compCount);
    std::iota(par.begin(), par.end(), 0);
    std::function<int(int)> find = [&](int a) {
        while (a != par[a]) { par[a] = par[par[a]]; a = par[a]; }
        return a;
    };
    auto unite = [&](int a, int b) {
        a = find(a); b = find(b);
        if (a != b) par[b] = a;
    };

    const int dirs6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    std::unordered_map<int, int> sandwichAssign; // natural block index → a component it joins

    for (int i = 0; i < n; i++)
    {
        if (comp[i] < 0) continue;
        const Key &k = pos[i];
        for (auto &d : dirs6)
        {
            Key n1{k.x + d[0], k.y + d[1], k.z + d[2]};
            auto it1 = idx.find(n1);
            if (it1 == idx.end()) continue;
            int j = it1->second;

            if (comp[j] >= 0)
            {
                unite(comp[i], comp[j]); // two buildings touch directly
            }
            else
            {
                // sandwiched natural block: check one step further in the same line
                Key n2{k.x + 2 * d[0], k.y + 2 * d[1], k.z + 2 * d[2]};
                auto it2 = idx.find(n2);
                if (it2 == idx.end()) continue;
                int m = it2->second;
                if (comp[m] >= 0 && find(comp[m]) != find(comp[i]))
                {
                    unite(comp[i], comp[m]);
                    sandwichAssign[j] = comp[i];
                }
            }
        }
    }

    // contiguous labels for the roots that still own blocks (skip discarded/empty)
    std::unordered_map<int, int> label;
    int buildingCount = 0;
    auto labelOf = [&](int root) -> int {
        auto it = label.find(root);
        if (it != label.end()) return it->second;
        return label[root] = buildingCount++;
    };
    for (int i = 0; i < n; i++)
        if (comp[i] >= 0)
            labelOf(find(comp[i]));
    for (auto &kv : sandwichAssign)
        labelOf(find(kv.second));

    std::cout << "After merge: " << buildingCount << " buildings" << std::endl;

    // final per-coordinate marker prop (the building's colour id for the renderer)
    std::unordered_map<Key, int, KeyHash> markerAt;
    for (int i = 0; i < n; i++)
        if (comp[i] >= 0)
            markerAt[pos[i]] = label[find(comp[i])];
    for (auto &kv : sandwichAssign)
        markerAt[pos[kv.first]] = label[find(kv.second)];

    std::cout << "Marking " << markerAt.size() << " building blocks" << std::endl;

    // ── derive output names + marker id ───────────────────────────────────────
    std::string outStem = worldName;
    if (endsWith(outStem, ".world"))
        outStem = outStem.substr(0, outStem.size() - 6);

    int buildingMarkerID = maxBlockID + 1;

    // widen the id/prop field if either the new marker id OR the largest building
    // colour id (stored in prop) no longer fits the original width
    int outIDBytes = ID_PROP_BYTES;
    auto fits = [](int v, int bytes) -> bool {
        if (bytes >= 8) return true;
        return (uint64_t)v < (1ULL << (bytes * 8));
    };
    while (!fits(buildingMarkerID, outIDBytes) || !fits(buildingCount, outIDBytes))
        outIDBytes++;

    // ── write the buildings world (markers substituted in place) ──────────────
    {
        auto wLE = [](gzFile f, uint64_t val, int nb) {
            uint8_t buf[8];
            for (int i = 0; i < nb; i++) buf[i] = (uint8_t)((val >> (8 * i)) & 0xFF);
            gzwrite(f, buf, (unsigned)nb);
        };
        auto enc = [](int val, int nb) -> uint64_t {
            return val < 0 ? ((uint64_t)(-val) | (1ULL << (nb * 8 - 1))) : (uint64_t)val;
        };

        gzFile outGz = gzopen((outStem + "_buildings.world").c_str(), "wb");
        if (customWorld) gzputs(outGz, ("namespace bytes: " + std::to_string(NS_BYTES) + "\n").c_str());
        gzputs(outGz, ("id/prop bytes: " + std::to_string(outIDBytes) + "\n").c_str());
        gzputs(outGz, ("x coord bytes: " + std::to_string(X_BYTES) + "\n").c_str());
        gzputs(outGz, ("y coord bytes: " + std::to_string(Y_BYTES) + "\n").c_str());
        gzputs(outGz, ("z coord bytes: " + std::to_string(Z_BYTES) + "\n").c_str());

        gzseek(gz, binaryStart, SEEK_SET);
        std::vector<uint8_t> buf(BLOCK_SIZE);
        while (gzread(gz, buf.data(), BLOCK_SIZE) == BLOCK_SIZE)
        {
            int off = coordsOff;
            int bx = decodeCoord(readLE(buf.data() + off, X_BYTES), X_BYTES); off += X_BYTES;
            int by = decodeCoord(readLE(buf.data() + off, Y_BYTES), Y_BYTES); off += Y_BYTES;
            int bz = decodeCoord(readLE(buf.data() + off, Z_BYTES), Z_BYTES);

            int outType, outProp;
            auto mit = markerAt.find(Key{bx, by, bz});
            if (mit != markerAt.end())
            {
                outType = buildingMarkerID;
                outProp = mit->second; // renderer reads the building colour id from prop
            }
            else
            {
                outType = (int)readLE(buf.data() + typeOff, ID_PROP_BYTES);
                outProp = (int)readLE(buf.data() + typeOff + ID_PROP_BYTES, ID_PROP_BYTES);
            }

            if (customWorld) wLE(outGz, readLE(buf.data(), NS_BYTES), NS_BYTES);
            wLE(outGz, (uint64_t)outType, outIDBytes);
            wLE(outGz, (uint64_t)outProp, outIDBytes);
            wLE(outGz, enc(bx, X_BYTES), X_BYTES);
            wLE(outGz, enc(by, Y_BYTES), Y_BYTES);
            wLE(outGz, enc(bz, Z_BYTES), Z_BYTES);
        }
        gzclose(outGz);
    }

    // ── write blockIds with the building_marker entry appended ────────────────
    {
        blockIDs.clear();
        blockIDs.seekg(0);
        std::ofstream dst(outStem + "_buildings.blockIds");
        dst << blockIDs.rdbuf();
        dst << buildingMarkerID << "=building_marker\n";
    }

    std::cout << "Building data written to " << outStem << "_buildings.world" << std::endl;

    gzclose(gz);
    blockIDs.close();
    return 0;
}
