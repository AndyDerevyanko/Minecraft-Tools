import amulet
from amulet.api.errors import ChunkLoadError, ChunkDoesNotExist
from amulet.api.block import Block
import time
import os
import gc

filename = input("Input World Path (ex. C:\\Users\\username\\AppData\\Roaming\\.minecraft\\saves\\world): ")

level = amulet.load_level(filename)

filename = os.path.basename(filename)
                          
game_version = []

game_version.append(input("Enter platform (Java/Bedrock): ").lower())

game_version.append(input("Enter game version: ").split(".")[:3])

dimension = input("Enter dimension WITH PREFIX (ex. minecraft:overworld): ").lower()

upper_lim = int(input("Enter the max height to which should be scanned (Usually 256 or 320 depending on version) (exclusive): "))
               
lower_lim = int(input("Enter the min height to which should be scanned (Usually 0 or -64 depending on version) (inclusive): "))

all = True if input("Scan entire world or define Horizontal limits? (y: Scan whole world, n: Define limits): ").lower()== "y" else False

if not all:
    lowerX_lim = int(input("Enter the min X (inclusive):"))
            
    upperX_lim = int(input("Enter the max X (inclusive):"))

    lowerZ_lim = int(input("Enter the min Z (inclusive):"))
               
    upperZ_lim = int(input("Enter the max Z (inclusive):"))

custom = True if input("Are there custom blocks in your world? (Y/N): ").lower()== "y" else False
            
block_id_dict= {}
block_property_dict= {}
block_namespace_dict = {}

block_id_counter = 0
block_property_counter = 0
block_namespace_counter = 0

output_lines = []

total_chunks = 0

for cx, cz in level.all_chunk_coords(dimension):
    if not all:
        if cx*16 + 15 >= lowerX_lim and cx*16 <= upperX_lim and cz*16 + 15 >= lowerZ_lim and cz*16 <= upperZ_lim:
            total_chunks+=1
    else:
        total_chunks+=1
        

f = open(f"{filename}.txt", "w")


last_time = time.time()


for n, (cx, cz) in enumerate(level.all_chunk_coords(dimension), 1):
    if not all:
        #chunks outside boundary
        if not(cx*16 + 15 >= lowerX_lim and cx*16 <= upperX_lim and cz*16 + 15 >= lowerZ_lim and cz*16 <= upperZ_lim):
            continue
        #border chunks
        else:
            check_chunk_lowerOX = False
            check_chunk_upperOX = False
            check_chunk_lowerOZ = False
            check_chunk_upperOZ = False
            
            if cx*16 + 15 > upperX_lim:
                upperOX_lim = upperX_lim - 16*cx
                check_chunk_upperOX = True
            if cx*16 < lowerX_lim:
                lowerOX_lim = lowerX_lim - 16*cx
                check_chunk_lowerOX = True
            if cz*16 + 15 > upperZ_lim:
                upperOZ_lim = upperZ_lim - 16*cz
                check_chunk_upperOZ = True
            if cz*16 < lowerZ_lim:
                lowerOZ_lim = lowerZ_lim - 16*cz
                check_chunk_lowerOZ = True      
    try:
        chunk = level.get_chunk(cx, cz, dimension)
    except (ChunkLoadError, ChunkDoesNotExist):
        continue
    
    for i in reversed(lower_lim,upper_lim):
        for Oz in range(0,16) if all else range(0 if not check_chunk_lowerOZ else lowerOZ_lim, 16 if not check_chunk_upperOZ else upperOZ_lim + 1):
            for Ox in range(0,16) if all else range(0 if not check_chunk_lowerOX else lowerOX_lim, 16 if not check_chunk_upperOX else upperOX_lim + 1):
                block = chunk.get_block(Ox, i, Oz)
                
                if(block.base_name !="air"):
                    if block.base_name not in block_id_dict:
                        block_id_dict[block.base_name] = block_id_counter
                        block_id_counter += 1
                        
                    indexA = block_id_dict[block.base_name]

                    if block.properties:
                        properties_key = str(block.properties)
                        if properties_key not in block_property_dict:
                            block_property_dict[properties_key] = block_property_counter
                            block_property_counter += 1

                        if custom:
                            namespace_key = str(block.namespace)
                            if namespace_key not in block_namespace_dict:
                                block_namespace_dict[namespace_key] = block_namespace_counter
                                block_namespace_counter += 1
                            output_lines.append(f"{block_namespace_dict[namespace_key]}:{indexA}:{block_property_dict[properties_key]}:{Ox + cx*16}.{i}.{Oz + cz*16}\n")
                        else:
                            output_lines.append(f"{indexA}:{block_property_dict[properties_key]}:{Ox + cx*16}.{i}.{Oz + cz*16}\n")
                            
                            
                    else:
                        if custom:
                            namespace_key = str(block.namespace)
                            if namespace_key not in block_namespace_dict:
                                block_namespace_dict[namespace_key] = block_namespace_counter
                                block_namespace_counter += 1
                            output_lines.append(f"{block_namespace_dict[namespace_key]}:{indexA}::{Ox + cx*16}.{i}.{Oz + cz*16}\n")
                        else:
                            output_lines.append(f"{indexA}::{Ox + cx*16}.{i}.{Oz + cz*16}\n")
    if n % 50 == 0:
        curr_time = time.time()
        passed = curr_time - last_time
        print(f"chunk {round(n/total_chunks*100, 2)}% complete ({n}/{total_chunks}) - took {passed:.2f} seconds")
        last_time = curr_time

    f.writelines(output_lines)
    output_lines = []

    if n % 200 == 0:
        level.unload_unchanged()

f.close()            

print("Creating BlockID key file\n")
g = open(f"{filename}blockIds.txt", "w")
for key, value in block_id_dict.items():
    g.write(f"{key}={value}\n")
g.close()        

print("Creating BlockProperties key file\n")
h = open(f"{filename}blockProperties.txt", "w")
for key, value in block_property_dict.items():
    h.write(f"{key}={value}\n")
h.close()

if custom:
    print("Creating BlockNamespaces key file\n")
    k = open(f"{filename}blockNamespaces.txt", "w")
    for key, value in block_namespace_dict.items():
        k.write(f"{key}={value}\n")
    k.close()

# close the world
level.close()

input("COMPLETE! Press <enter> to close...")

exit(0)
