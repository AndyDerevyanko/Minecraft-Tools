# Exports a voxel mesh w/ culled internal faces to debug_model_voxels.obj

import os
import binaryReader

IN_PATH = input("Enter path of world file fully: ").strip().strip('\"').strip('\'').rstrip('\\').strip()
world_name = os.path.basename(IN_PATH)

OUT_OBJ = os.path.dirname(IN_PATH) + f"\\{world_name}.obj"

if not os.path.exists(IN_PATH):
    print("ERROR: file not found")
    exit(1)

custom = True if input("Does your world have any custom (modded) blocks? : ").lower()== "y" else False

(INDICES, (BLOCK_SIZE, CHUNK, *BYTES)) \
= binaryReader.setUpInternalParams(os.path.dirname(IN_PATH), world_name, custom) 

voxels = set()
with open(IN_PATH, "br") as world_file:

    for _ in range(4 + int(custom)): #go over first 5 lines if custom, else 4 (our header needs to be skipped)
        world_file.readline()

    while True:
        blocks = world_file.read(CHUNK*64) #read 64 chunks at a time

        if not blocks:
            break

        for block_idx in range(0, len(blocks), BLOCK_SIZE):
             voxels.add(binaryReader.getBlockCoords(blocks, block_idx, INDICES, BYTES))

if not voxels:
    print("No voxel lines parsed (check pattern). Exiting.")
    exit(1)

# cube corners and faces
corners = [
    (0,0,0),(1,0,0),(1,1,0),(0,1,0),
    (0,0,1),(1,0,1),(1,1,1),(0,1,1)
]

faces_def = [
    ([0,1,2,3], (0,0,-1)),  # -Z
    ([4,5,6,7], (0,0,1)),   # +Z
    ([0,1,5,4], (0,-1,0)),  # -Y
    ([3,2,6,7], (0,1,0)),   # +Y
    ([0,3,7,4], (-1,0,0)),  # -X
    ([1,2,6,5], (1,0,0)),   # +X
]

vertices = []
faces = []
vert_index = {}

def get_vert(v):
    if v not in vert_index:
        vert_index[v] = len(vertices) + 1
        vertices.append(v)
    return vert_index[v]

for (x,y,z) in voxels:
    for face_corners, offset in faces_def:
        nx, ny, nz = x + offset[0], y + offset[1], z + offset[2]
        if (nx, ny, nz) in voxels:
            continue  # internal face hidden
        idxs = []
        for ci in face_corners:
            cx, cy, cz = corners[ci]
            idxs.append(get_vert((x+cx, y+cy, z+cz)))
        faces.append(tuple(idxs))

with open(OUT_OBJ, "w") as f:
    f.write("# generated from debugOut.txt\n")
    for v in vertices:
        f.write(f"v {v[0]} {v[1]} {v[2]}\n")
    for fa in faces:
        f.write(f"f {fa[0]} {fa[1]} {fa[2]} {fa[3]}\n")

print(f"SUCESS: Voxels: {len(voxels)}, Vertices: {len(vertices)}, Faces: {len(faces)}")
print(f"OBJ written to: {OUT_OBJ}")