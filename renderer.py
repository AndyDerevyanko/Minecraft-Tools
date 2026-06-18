# Exports a colored voxel mesh w/ culled internal faces to {world_name}.obj / .mtl

import os
import gzip
import random
import colorsys

IN_PATH = input("Enter path of world file fully: ").strip().strip('"').strip("'").rstrip('\\').strip()
world_name = os.path.basename(IN_PATH)
world_base = os.path.splitext(world_name)[0]
folder = os.path.dirname(IN_PATH)

OUT_OBJ = folder + f"\\{world_base}.obj"
OUT_MTL = folder + f"\\{world_base}.mtl"
IDS_PATH = folder + f"\\{world_base}.blockIds"

if not os.path.exists(IN_PATH):
    print("ERROR: world file not found")
    exit(1)

if not os.path.exists(IDS_PATH):
    print(f"ERROR: blockIds file not found at {IDS_PATH}")
    exit(1)

custom = True if input("Does your world have any custom (modded) blocks? : ").lower() == "y" else False

# read binary header (open in binary mode to avoid codec errors from binary block data)
with gzip.open(IN_PATH, "rb") as hdr:
    grab = lambda: int(hdr.readline().decode('ascii').split(":")[1].strip())
    NS_BYTES      = grab() if custom else 0
    ID_PROP_BYTES = grab()
    X_BYTES       = grab()
    Y_BYTES       = grab()
    Z_BYTES       = grab()

BLOCK_SIZE = NS_BYTES + 2 * ID_PROP_BYTES + X_BYTES + Y_BYTES + Z_BYTES
IDS_OFF    = NS_BYTES
COORDS_OFF = NS_BYTES + 2 * ID_PROP_BYTES
CHUNK      = BLOCK_SIZE * 4096

# block id → name
id_to_name = {}
with open(IDS_PATH) as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        pos = line.find('=')
        if pos == -1:
            continue
        id_to_name[int(line[:pos])] = line[pos + 1:]

# categorise exactly as treeRemover.cpp does
STUMPABLE_NAMES = {
    "dirt", "grass_block", "coarse_dirt", "podzol", "rooted_dirt",
    "moss_block", "pale_moss_block", "mud", "muddy_mangrove_roots",
    "mycelium", "farmland", "clay", "crimson_nylium", "warped_nylium",
    "gravel",
}

def categorise(name):
    if name == "tree_marker":
        return "tree_marker"
    if name == "foliage_marker":
        return "foliage_marker"
    if name == "building_marker":
        return "building"
    if name.endswith("_leaves"):
        return "leaf"
    if (name.endswith("_log") or name.endswith("_wood") or name.endswith("_hyphae")
            or name in ("crimson_stem", "warped_stem")):
        return "wood"
    if name in STUMPABLE_NAMES:
        return "stumpable"
    return "other"

id_cat = {nid: categorise(name) for nid, name in id_to_name.items()}

def decode_coord(raw, nbytes):
    sign_bit = 1 << (nbytes * 8 - 1)
    return -(raw & ~sign_bit) if (raw & sign_bit) else raw

# read all blocks
voxels = {}  # (x,y,z) -> category
with gzip.open(IN_PATH, "rb") as wf:
    for _ in range(4 + int(custom)):
        wf.readline()
    while True:
        data = wf.read(CHUNK * 64)
        if not data:
            break
        for bi in range(0, len(data), BLOCK_SIZE):
            block_id = int.from_bytes(data[bi + IDS_OFF : bi + IDS_OFF + ID_PROP_BYTES], "little")
            off = bi + COORDS_OFF
            x = decode_coord(int.from_bytes(data[off : off + X_BYTES], "little"), X_BYTES); off += X_BYTES
            y = decode_coord(int.from_bytes(data[off : off + Y_BYTES], "little"), Y_BYTES); off += Y_BYTES
            z = decode_coord(int.from_bytes(data[off : off + Z_BYTES], "little"), Z_BYTES)
            voxels[(x, y, z)] = id_cat.get(block_id, "other")

if not voxels:
    print("No blocks found. Exiting.")
    exit(1)

# colour each connected structure its own random green→blue shade. Building
# blocks are flood-filled into components via 6-connectivity (face-adjacent),
# and every block in a component is retagged with that component's material.
building_mats = {}  # material name -> (r,g,b)
building_set = {pos for pos, cat in voxels.items() if cat == "building"}
if building_set:
    NEIGH = [(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)]
    seen = set()
    comp_id = 0
    for start in building_set:
        if start in seen:
            continue
        stack = [start]
        seen.add(start)
        comp = []
        while stack:
            cx, cy, cz = stack.pop()
            comp.append((cx, cy, cz))
            for dx, dy, dz in NEIGH:
                npos = (cx + dx, cy + dy, cz + dz)
                if npos in building_set and npos not in seen:
                    seen.add(npos)
                    stack.append(npos)
        mat = f"building_{comp_id}"
        hue = random.uniform(0.333, 0.667)  # 0.333 green → 0.667 blue
        building_mats[mat] = colorsys.hsv_to_rgb(hue, 0.90, 0.95)
        for p in comp:
            voxels[p] = mat
        comp_id += 1

# material colours
MATERIALS = {
    "tree_marker":    (0.50, 0.00, 0.80),  # purple  — confirmed tree stumps
    "foliage_marker": (1.00, 0.00, 0.00),  # red     — unconfirmed foliage
    "leaf":           (0.13, 0.55, 0.13),  # green   — undetected leaves
    "wood":           (0.40, 0.26, 0.13),  # brown   — undetected wood
    "stumpable":      (0.40, 0.40, 0.40),  # dark grey
    "other":          (0.72, 0.72, 0.72),  # light grey
}
MATERIALS.update(building_mats)  # per-structure green→blue materials

with open(OUT_MTL, "w") as f:
    for mat, (r, g, b) in MATERIALS.items():
        f.write(f"newmtl {mat}\nKd {r:.3f} {g:.3f} {b:.3f}\n\n")

# build mesh
corners = [
    (0,0,0),(1,0,0),(1,1,0),(0,1,0),
    (0,0,1),(1,0,1),(1,1,1),(0,1,1),
]
faces_def = [
    ([0,1,2,3], (0,0,-1)),
    ([4,5,6,7], (0,0,1)),
    ([0,1,5,4], (0,-1,0)),
    ([3,2,6,7], (0,1,0)),
    ([0,3,7,4], (-1,0,0)),
    ([1,2,6,5], (1,0,0)),
]

voxel_set   = set(voxels)
vertices    = []
vert_index  = {}
faces_by_mat = {m: [] for m in MATERIALS}

def get_vert(v):
    if v not in vert_index:
        vert_index[v] = len(vertices) + 1
        vertices.append(v)
    return vert_index[v]

for (x, y, z), cat in voxels.items():
    for fc, (ox, oy, oz) in faces_def:
        if (x + ox, y + oy, z + oz) in voxel_set:
            continue
        faces_by_mat[cat].append(
            tuple(get_vert((x + corners[ci][0], y + corners[ci][1], z + corners[ci][2])) for ci in fc)
        )

mtl_file = os.path.basename(OUT_MTL)
with open(OUT_OBJ, "w") as f:
    f.write(f"mtllib {mtl_file}\n")
    for v in vertices:
        f.write(f"v {v[0]} {v[1]} {v[2]}\n")
    for mat, faces in faces_by_mat.items():
        if not faces:
            continue
        f.write(f"usemtl {mat}\n")
        for fa in faces:
            f.write(f"f {fa[0]} {fa[1]} {fa[2]} {fa[3]}\n")

total_faces = sum(len(v) for v in faces_by_mat.values())
print(f"SUCCESS: Voxels: {len(voxels)}, Vertices: {len(vertices)}, Faces: {total_faces}")
print(f"OBJ written to: {OUT_OBJ}")
print(f"MTL written to: {OUT_MTL}")
