# Reads a "<base>_trees_buildings.world" file, groups every building_marker
# block by its component (stored in the marker's prop field), recovers each
# block's REAL type + orientation by cross-referencing the pre-buildingCatcher
# "<base>_trees.world" at the same coordinate (buildingCatcher overwrites every
# building block's type/prop, so the original identity only survives there),
# and emits a single self-contained HTML file with a Three.js viewer that lets
# you step through the structures one at a time.

import os
import sys
import gzip
import json
import webbrowser

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TEMPLATE_PATH = os.path.join(SCRIPT_DIR, "structureViewer_template.html")


# ── binary world reading (same scheme as renderer.py / the C++ tools) ────────

def decode_coord(raw, nbytes):
    sign_bit = 1 << (nbytes * 8 - 1)
    return -(raw & ~sign_bit) if (raw & sign_bit) else raw


def read_world(path, custom):
    with gzip.open(path, "rb") as f:
        grab = lambda: int(f.readline().decode("ascii").split(":")[1].strip())
        ns_bytes = grab() if custom else 0
        id_bytes = grab()
        x_bytes = grab()
        y_bytes = grab()
        z_bytes = grab()
        block_size = ns_bytes + 2 * id_bytes + x_bytes + y_bytes + z_bytes
        ids_off = ns_bytes
        coords_off = ns_bytes + 2 * id_bytes

        blocks = {}  # (x,y,z) -> (type, prop)
        chunk = block_size * 4096
        while True:
            data = f.read(chunk)
            if not data:
                break
            for bi in range(0, len(data) - block_size + 1, block_size):
                btype = int.from_bytes(data[bi + ids_off: bi + ids_off + id_bytes], "little")
                bprop = int.from_bytes(data[bi + ids_off + id_bytes: bi + ids_off + 2 * id_bytes], "little")
                off = bi + coords_off
                x = decode_coord(int.from_bytes(data[off:off + x_bytes], "little"), x_bytes); off += x_bytes
                y = decode_coord(int.from_bytes(data[off:off + y_bytes], "little"), y_bytes); off += y_bytes
                z = decode_coord(int.from_bytes(data[off:off + z_bytes], "little"), z_bytes)
                blocks[(x, y, z)] = (btype, bprop)
    return blocks


def read_block_ids(path):
    id_to_name = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            pos = line.find("=")
            if pos == -1:
                continue
            id_to_name[int(line[:pos])] = line[pos + 1:]
    return id_to_name


def read_block_properties(path):
    """propID -> dict of key/val pairs, e.g. {'facing':'north','half':'bottom'}."""
    prop_to_kv = {}
    if not os.path.isfile(path):
        return prop_to_kv
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            pos = line.find("=")
            if pos == -1:
                continue
            prop_id = int(line[:pos])
            rest = line[pos + 1:]
            kv = {}
            if rest != "None":
                for pair in rest.split(","):
                    if "=" in pair:
                        k, v = pair.split("=", 1)
                        kv[k] = v
            prop_to_kv[prop_id] = kv
    return prop_to_kv


def is_custom_world(path):
    with gzip.open(path, "rb") as f:
        first = f.readline().decode("ascii", "replace")
    return "namespace" in first


# ── shape classification ──────────────────────────────────────────────────
# Maps a block name to a shapeId the JS geometry generator understands.
# Falls back to a plain cube for anything unrecognised.

def ends_any(name, suffixes):
    return any(name.endswith(s) for s in suffixes)


def classify_shape(name):
    if name in ("building_marker", "tree_marker", "foliage_marker"):
        return "cube"

    if ends_any(name, ("_slab",)):
        return "slab"
    if ends_any(name, ("_stairs",)):
        return "stair"
    if ends_any(name, ("_fence",)) and not ends_any(name, ("_fence_gate",)):
        return "fence"
    if ends_any(name, ("_fence_gate",)):
        return "fence_gate"
    if ends_any(name, ("_wall",)) and "torch" not in name:
        return "wall"
    if name in ("glass_pane",) or ends_any(name, ("_glass_pane",)):
        return "pane"
    if name == "iron_bars":
        return "pane"
    if ends_any(name, ("_door",)):
        return "door"
    if ends_any(name, ("_trapdoor",)):
        return "trapdoor"
    if ends_any(name, ("_wall_torch",)):
        return "wall_torch"
    if ends_any(name, ("_torch",)):
        return "torch"
    if ends_any(name, ("_carpet",)):
        return "carpet"
    if ends_any(name, ("_pressure_plate",)):
        return "pressure_plate"
    if ends_any(name, ("_button",)):
        return "button"
    if name == "ladder":
        return "ladder"
    if ends_any(name, ("_bed",)):
        return "bed"
    if ends_any(name, ("_wall_sign",)):
        return "wall_sign"
    if ends_any(name, ("_sign",)):
        return "sign"
    if name in ("rail", "powered_rail", "detector_rail", "activator_rail"):
        return "rail"
    if name in ("snow", "lily_pad") or name.startswith("snow") and name != "snow_block":
        return "layer"
    if name == "cactus":
        return "inset_cube"
    if name in ("chain",):
        return "chain"
    if name in ("cobweb",):
        return "cross"
    if (
        ends_any(name, ("_sapling",))  # leaves are real full-cube blocks, NOT billboards
        or name
        in (
            "short_grass", "grass", "tall_grass", "fern", "large_fern", "dead_bush",
            "dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "red_tulip",
            "orange_tulip", "white_tulip", "pink_tulip", "oxeye_daisy", "cornflower",
            "lily_of_the_valley", "wither_rose", "torchflower", "sunflower", "lilac",
            "rose_bush", "peony", "pitcher_plant", "sweet_berry_bush", "nether_wart",
            "crimson_roots", "warped_roots", "nether_sprouts", "twisting_vines",
            "twisting_vines_plant", "weeping_vines", "weeping_vines_plant",
            "cave_vines", "cave_vines_plant", "vine", "glow_lichen", "hanging_roots",
            "spore_blossom", "big_dripleaf", "small_dripleaf", "pink_petals",
            "kelp", "kelp_plant", "seagrass", "tall_seagrass", "wheat", "carrots",
            "potatoes", "beetroots", "bamboo", "sugar_cane", "azalea",
            "flowering_azalea", "bush", "firefly_bush", "leaf_litter",
        )
    ):
        return "cross"
    return "cube"


# ── color palette ─────────────────────────────────────────────────────────
# Single flat RGB color per block "family". Returns (color, topColor|None) —
# topColor is set only for blocks whose top face is a different color in real
# Minecraft (grass_block, mycelium, podzol) so the cube still reads correctly
# even though everything is flat-shaded.

DYE_COLORS = {
    "white": "#e9ecec", "orange": "#f07613", "magenta": "#bf4ba7", "light_blue": "#3cabd6",
    "yellow": "#f6c01b", "lime": "#70b819", "pink": "#ed8dac", "gray": "#3e4447",
    "light_gray": "#8e8e86", "cyan": "#158992", "purple": "#792aac", "blue": "#35399c",
    "brown": "#664c33", "green": "#546c1d", "red": "#a02722", "black": "#191616",
}

WOOD_COLORS = {
    "oak": "#9c7a4a", "spruce": "#5b3a23", "birch": "#d8c98f", "jungle": "#9c6b4a",
    "acacia": "#bb5b32", "dark_oak": "#412c1d", "mangrove": "#6f3326", "cherry": "#e3b7c0",
    "pale_oak": "#cfc6a8", "bamboo": "#b9b94a", "crimson": "#7c324a", "warped": "#287f78",
}

STONE_COLORS = {
    "stone": "#8c8c8c", "cobblestone": "#7a7a7a", "mossy_cobblestone": "#74815f",
    "stone_bricks": "#888888", "mossy_stone_bricks": "#7e8a6e", "cracked_stone_bricks": "#7f7f7f",
    "granite": "#946a5a", "diorite": "#bdbdbd", "andesite": "#888a8c",
    "deepslate": "#454649", "deepslate_bricks": "#3f4043", "tuff": "#6b6c5f",
    "calcite": "#dde0d8", "blackstone": "#2b2429", "basalt": "#54514f",
    "smooth_basalt": "#535355", "bricks": "#9a5236", "obsidian": "#15101c",
    "crying_obsidian": "#2a0e3c", "bedrock": "#5a5a5a", "gravel": "#8b8378",
    "quartz_block": "#ece6da", "purpur_block": "#a276a8", "end_stone": "#dcdca7",
    "netherrack": "#6b3a3a", "nether_bricks": "#2c1818", "soul_sand": "#52402f",
    "soul_soil": "#4a382a", "crimson_nylium": "#7a1f1f", "warped_nylium": "#147c6f",
    "prismarine": "#5fa399", "prismarine_bricks": "#63ada0", "dark_prismarine": "#345e4e",
}

ORE_TINTS = {
    "coal_ore": "#363636", "iron_ore": "#af9a86", "copper_ore": "#9a5a3c",
    "gold_ore": "#dec33d", "redstone_ore": "#8c2020", "lapis_ore": "#2742a8",
    "diamond_ore": "#79e6e0", "emerald_ore": "#1fae5c",
    "deepslate_coal_ore": "#39393b", "deepslate_iron_ore": "#9a8978",
    "deepslate_copper_ore": "#8a5640", "deepslate_gold_ore": "#c2ab40",
    "deepslate_redstone_ore": "#7e2626", "deepslate_lapis_ore": "#33458e",
    "deepslate_diamond_ore": "#6bbdb8", "deepslate_emerald_ore": "#2c8f57",
    "nether_gold_ore": "#c98a3e", "nether_quartz_ore": "#a6776a",
    "ancient_debris": "#5a4438", "raw_iron_block": "#c2a384", "raw_copper_block": "#a8643f",
    "raw_gold_block": "#dcb73a",
}

LIQUID_COLORS = {"water": "#2a5fb0", "lava": "#d8540f"}
ICE_COLORS = {"ice": "#7fb6e8", "packed_ice": "#9bc8ee", "blue_ice": "#74c3e6", "frosted_ice": "#a9d4ee"}


def hex_for_wood(name):
    for variety, color in WOOD_COLORS.items():
        if name.startswith(variety):
            return color
    return None


def hex_for_dye(name):
    for dye, color in DYE_COLORS.items():
        if name.startswith(dye + "_") or name == dye:
            return color
    return None


def base_color(name):
    """Returns (color_hex, top_color_hex_or_None)."""

    if name in ("tree_marker",):
        return "#7f00cc", None
    if name in ("foliage_marker",):
        return "#ff0000", None
    if name == "building_marker":
        return "#3399ff", None

    if name == "grass_block":
        return "#7a6243", "#5d8a3c"
    if name == "mycelium":
        return "#6b5b6e", "#8a7a8e"
    if name == "podzol":
        return "#5a4226", "#5d4a2e"
    if name in ("dirt_path", "grass_path"):
        return "#9c8158", "#8a9a55"

    # dyed categories
    if (
        ends_any(name, ("_wool", "_carpet", "_concrete", "_concrete_powder",
                         "_stained_glass", "_stained_glass_pane", "_terracotta",
                         "_glazed_terracotta", "_candle", "_bed", "_banner",
                         "_shulker_box"))
    ):
        c = hex_for_dye(name)
        if c:
            return c, None

    if name == "terracotta":
        return "#985f49", None

    wood = hex_for_wood(name)
    if wood:
        return wood, None

    if name in STONE_COLORS:
        return STONE_COLORS[name], None
    # stairs/slabs/walls/fences of a stone family inherit that family's color
    for stem, color in STONE_COLORS.items():
        if name.startswith(stem):
            return color, None

    if name in ORE_TINTS:
        return ORE_TINTS[name], None

    if name in LIQUID_COLORS:
        return LIQUID_COLORS[name], None
    if name in ICE_COLORS:
        return ICE_COLORS[name], None

    if ends_any(name, ("_leaves",)):
        if name.startswith("azalea") or name.startswith("flowering_azalea"):
            return "#4f7a2c", None
        return "#3f7d33", None
    if name in ("short_grass", "grass", "tall_grass", "fern", "large_fern"):
        return "#5d9c3a", None
    if name in ("dead_bush",):
        return "#7a5a32", None
    if name in ("sand", "sandstone") or name.startswith("sandstone") or name.startswith("smooth_sandstone") \
            or name.startswith("cut_sandstone") or name.startswith("chiseled_sandstone"):
        return "#dccd8c", None
    if name in ("red_sand",) or name.startswith("red_sandstone") or name.startswith("smooth_red_sandstone") \
            or name.startswith("cut_red_sandstone") or name.startswith("chiseled_red_sandstone"):
        return "#a85a2e", None
    if name in ("dirt", "coarse_dirt", "rooted_dirt", "farmland", "mud", "packed_mud", "muddy_mangrove_roots"):
        return "#6e4a2c", None
    if name == "clay":
        return "#9aa1ad", None
    if name in ("snow", "snow_block", "powder_snow"):
        return "#f3f6f8", None
    if name.startswith("amethyst") or name.endswith("amethyst_bud") or name.endswith("amethyst_cluster"):
        return "#9a6fd6", None
    if name.startswith("glass") or name == "glass_pane":
        return "#bfe6f0", None
    if name.startswith("quartz"):
        return "#ece6da", None

    # unrecognised block — flat mid-grey fallback
    return "#a8a8a8", None


# ── relevant property keys passed through to the JS geometry generators ────
RELEVANT_PROP_KEYS = {
    "facing", "half", "type", "shape", "hinge", "open", "axis",
    "north", "south", "east", "west", "rotation", "part", "powered",
}


def filtered_props(kv):
    return {k: v for k, v in kv.items() if k in RELEVANT_PROP_KEYS}


# ── main extraction ─────────────────────────────────────────────────────────

def derive_paths(buildings_world_path):
    p = buildings_world_path
    suffix = "_trees_buildings.world"
    if not p.endswith(suffix):
        raise ValueError(f"expected a path ending in {suffix!r} (got {p!r})")
    base = p[: -len(suffix)]
    return {
        "base": base,
        "trees_world": base + "_trees.world",
        "trees_ids": base + "_trees.blockIds",
        "buildings_ids": base + "_trees_buildings.blockIds",
        "block_properties": base + ".blockProperties",
    }


def build_structures(buildings_world_path):
    paths = derive_paths(buildings_world_path)
    for key in ("trees_world", "trees_ids", "buildings_ids"):
        if not os.path.isfile(paths[key]):
            raise FileNotFoundError(f"missing required file: {paths[key]}")

    custom = is_custom_world(buildings_world_path)

    buildings_ids = read_block_ids(paths["buildings_ids"])
    name_to_id = {v: k for k, v in buildings_ids.items()}
    marker_id = name_to_id.get("building_marker")
    if marker_id is None:
        raise ValueError("buildings.blockIds has no building_marker entry")

    buildings_blocks = read_world(buildings_world_path, custom)
    trees_blocks = read_world(paths["trees_world"], custom)
    trees_ids = read_block_ids(paths["trees_ids"])
    prop_kv = read_block_properties(paths["block_properties"])

    # group coordinates by component id (the marker's prop field)
    components = {}
    for coord, (btype, bprop) in buildings_blocks.items():
        if btype != marker_id:
            continue
        components.setdefault(bprop, []).append(coord)

    structures = []
    for comp_id in sorted(components):
        coords = components[comp_id]
        blocks = []
        for (x, y, z) in coords:
            real = trees_blocks.get((x, y, z))
            if real is None:
                continue  # shouldn't happen, but don't crash on it
            rtype, rprop = real
            name = trees_ids.get(rtype, f"unknown_{rtype}")
            shape = classify_shape(name)
            color, top_color = base_color(name)
            kv = filtered_props(prop_kv.get(rprop, {}))
            block = {"x": x, "y": y, "z": z, "n": name, "s": shape, "c": color}
            if top_color:
                block["t"] = top_color
            if kv:
                block["p"] = kv
            blocks.append(block)
        if blocks:
            structures.append(make_structure(comp_id, blocks))

    return structures


# ── face culling for plain cubes ─────────────────────────────────────────────
# Plain full-cube blocks are the overwhelming majority of any structure, and a
# separate Mesh per block (one draw call each) is what made the viewer slow.
# Cull the faces shared between two adjacent cubes the same way renderer.py
# does for the whole-world .obj, and merge every remaining exposed quad of a
# given color into one triangle-soup mesh. Non-cube shapes (stairs, fences,
# etc.) are comparatively rare and keep their existing individual-mesh path.

_CUBE_FACES = [
    ((0, 0, -1), [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]),
    ((0, 0, 1), [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]),
    ((0, -1, 0), [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)]),
    ((0, 1, 0), [(0, 1, 0), (1, 1, 0), (1, 1, 1), (0, 1, 1)]),
    ((-1, 0, 0), [(0, 0, 0), (0, 1, 0), (0, 1, 1), (0, 0, 1)]),
    ((1, 0, 0), [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)]),
]


def make_structure(comp_id, blocks):
    min_x = min(b["x"] for b in blocks); max_x = max(b["x"] for b in blocks)
    min_y = min(b["y"] for b in blocks); max_y = max(b["y"] for b in blocks)
    min_z = min(b["z"] for b in blocks); max_z = max(b["z"] for b in blocks)

    cube_blocks = [b for b in blocks if b["s"] == "cube"]
    other_blocks = [b for b in blocks if b["s"] != "cube"]
    occupied = {(b["x"], b["y"], b["z"]) for b in cube_blocks}

    # color hex -> (positions, normals), both flat float lists, 3 per vertex
    buckets = {}

    def bucket(color):
        if color not in buckets:
            buckets[color] = ([], [])
        return buckets[color]

    for b in cube_blocks:
        x, y, z = b["x"], b["y"], b["z"]
        side_color = b["c"]
        for (nx, ny, nz), corners in _CUBE_FACES:
            if (x + nx, y + ny, z + nz) in occupied:
                continue  # face touches another cube in this structure — hidden
            face_color = b["t"] if (b.get("t") and (nx, ny, nz) == (0, 1, 0)) else side_color
            pos, nrm = bucket(face_color)
            verts = [(x + cx, y + cy, z + cz) for cx, cy, cz in corners]
            # two triangles per quad (0,1,2) and (0,2,3); normal is identical per vertex
            for tri in ((verts[0], verts[1], verts[2]), (verts[0], verts[2], verts[3])):
                for vx, vy, vz in tri:
                    pos.extend((vx, vy, vz))
                    nrm.extend((nx, ny, nz))

    cube_meshes = [{"color": color, "pos": pos, "nrm": nrm} for color, (pos, nrm) in buckets.items()]

    return {
        "id": comp_id,
        "blocks": other_blocks,
        "cubeMeshes": cube_meshes,
        "bbox": [min_x, min_y, min_z, max_x, max_y, max_z],
        "count": len(blocks),
    }


def write_viewer(structures, world_base, out_path):
    with open(TEMPLATE_PATH, encoding="utf-8") as f:
        template = f.read()
    data_json = json.dumps(structures, separators=(",", ":"))
    html = template.replace("__STRUCTURES_JSON__", data_json).replace("__WORLD_NAME__", world_base)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(html)


def main():
    if len(sys.argv) > 1:
        path = sys.argv[1].strip().strip('"').strip("'")
    else:
        path = input("Enter path to a *_trees_buildings.world file: ").strip().strip('"').strip("'")

    if not os.path.isfile(path):
        print(f"ERROR: file not found: {path}")
        sys.exit(1)

    structures = build_structures(path)
    if not structures:
        print("No building components found in this world.")
        sys.exit(0)

    base = derive_paths(path)["base"]
    world_name = os.path.basename(base)
    out_path = base + "_structures.html"
    write_viewer(structures, world_name, out_path)

    total_blocks = sum(s["count"] for s in structures)
    print(f"Wrote {len(structures)} structure(s), {total_blocks} blocks total -> {out_path}")
    webbrowser.open("file://" + os.path.abspath(out_path))


if __name__ == "__main__":
    main()
