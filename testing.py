
import anvil
import time
import os
import json

folder_name = input("Input World Path (ex. C:\\Users\\username\\AppData\\Roaming\\.minecraft\\saves\\world): ")

folder_name = folder_name.strip('\"').strip('\'')

file_name = os.path.dirname(folder_name)

game_version = []

dimension = input("Enter dimension folder (if NOT custom dimension, use:  overworld: \'.\\\', nether: \'DIM-1\', end: \'DIM1\'): ").lower()

dimension = dimension.strip('\"').strip('\'').strip('\\').strip('/')

if dimension == ".":
    folder_name+= ("\\region")
else:
    folder_name+= ("\\" + dimension + "\\region")

mca_dir_list = [f for f in os.listdir(folder_name) if f.lower().endswith('.mca')] #make sure we use only mca files

upper_lim = int(input("Enter the max height to which should be scanned (Usually 256 or 320 depending on version) (exclusive): "))
               
lower_lim = int(input("Enter the min height to which should be scanned (Usually 0 or -64 depending on version) (inclusive): "))

scan_all = True if input("Scan entire world or define Horizontal limits? (Y: Scan whole world, N: Define limits): ").lower()== "y" else False

if not scan_all:
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

total_chunks = 0

for mca_file in mca_dir_list:
    region = anvil.Region.from_file(folder_name + "\\" + mca_file)

    region_x, region_z = map(int, mca_file.split(".")[1:3])

    for local_x in range(32):
        for local_z in range(32):
            if not scan_all:
                cx = 32 * region_x + local_x
                cz = 32 * region_z + local_z
                if cx*16 + 15 >= lowerX_lim and cx*16 <= upperX_lim and cz*16 + 15 >= lowerZ_lim and cz*16 <= upperZ_lim:
                    total_chunks+=1
            else:
                total_chunks+=1

n = 0 #counter for how many chunks we've gone thru
last_time = time.time()

for mca_file in mca_dir_list: # MCA scope
    region = anvil.Region.from_file(folder_name + "\\" + mca_file)
    
    region_x, region_z = map(int, mca_file.split(".")[1:3])

    for local_x in range(32):
        for local_z in range(32): #chunk scope

            cx = 32 * region_x + local_x
            cz = 32 * region_z + local_z

            if not scan_all:
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

            if region.chunk_location(local_x, local_z) is None:
                continue
            
            chunk = region.get_chunk(local_x, local_z)
            chunk_blks = list(chunk.stream_chunk())


            for section_y in range(lower_lim // 16, (upper_lim - 1) // 16 + 1):
                section = chunk.get_section(section_y)

                if not section:
                    continue
                
                is_air_only = False

                #1.18+ format:
                if 'block_states' in section and 'palette' in section['block_states']:
                    palette = section['block_states']['palette']
                    if len(palette) == 1 and palette[0]['Name'].value.endswith('air'):
                        is_air_only = True
                
                #1.17- format:
                elif 'Palette' in section:
                    palette = section['Palette']
                    if len(palette) == 1 and palette[0]['Name'].value.endswith('air'):
                        is_air_only = True

                if is_air_only:
                    continue

                section_base = section_y * 4096

                for idx in range(section_base, section_base + 4096): #go thru this section
                    block = chunk_blks[idx]

                    Ox, Oz, y = idx % 16, (idx // 16) % 16, idx // 256 #note that the index is arranged with 3d->1d address logic: index = 16*16*y + 16*z + x

                    if y < lower_lim or y >= upper_lim:
                        continue
                    
                    base_name = block.id

                    if base_name.endswith('air'):
                        continue 
                    
                    if scan_all:
                        if Oz < 0 or Oz >= 16:
                            continue

                        if Ox < 0 or Ox >= 16:
                            continue
                    else:
                        lowerZ = 0 if not check_chunk_lowerOZ else lowerOZ_lim
                        upperZ = 16 if not check_chunk_upperOZ else upperOZ_lim 
                        if Oz < lowerZ or Oz >= upperZ:
                            continue
                        
                        lowerX = 0 if not check_chunk_lowerOX else lowerOX_lim
                        upperX = 16 if not check_chunk_upperOX else upperOX_lim 
                        if Ox < lowerX or Ox >= upperX:
                            continue
                    
                    properties = block.properties

                    if properties:
                        #put off for later to be parsed into string (its a tuple of non-string tags)
                        properties = tuple(sorted(properties.items()))

                    namespace = block.namespace

                    if base_name not in block_id_dict:
                        block_id_dict[base_name] = block_id_counter
                        block_id_counter += 1
                        
                    indexA = block_id_dict[base_name]

                    if properties:
                        if properties not in block_property_dict:
                            block_property_dict[properties] = block_property_counter
                            block_property_counter += 1

                        if custom:
                            if namespace not in block_namespace_dict:
                                block_namespace_dict[namespace] = block_namespace_counter
                                block_namespace_counter += 1
                    else:
                        if custom:
                            if namespace not in block_namespace_dict:
                                block_namespace_dict[namespace] = block_namespace_counter
                                block_namespace_counter += 1
            n+=1

        # every 32 chunks update the user on progress:
        curr_time = time.time()
        passed = curr_time - last_time
        print(f"chunk {round(n/total_chunks*100, 2)}% complete ({n}/{total_chunks}) - took {passed:.2f} seconds")
        last_time = curr_time  


print("Creating BlockID key file\n")
g = open(f"{file_name}.blockIds.txt", "w")
for key, value in block_id_dict.items():
    g.write(f"{key}={value}\n")
g.close()        

print("Creating BlockProperties key file\n")
h = open(f"{file_name}.blockProperties.txt", "w")
for key, value in block_property_dict.items():
    items = [f"'{str(k)}' : {str(v)}" for k, v in key]
    inner_content = ", ".join(items)
    h.write(f"{{{inner_content}}}={value}\n")
h.close()

if custom:
    print("Creating BlockNamespaces key file\n")
    k = open(f"{file_name}.blockNamespaces.txt", "w")
    for key, value in block_namespace_dict.items():
        k.write(f"{key}={value}\n")
    k.close()

input("COMPLETE! Press <enter> to close...")

exit(0)

                          
