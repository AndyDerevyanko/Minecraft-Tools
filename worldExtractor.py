
import anvil
import time
import os
import json

def main():
    folder_name = input("Input World Path (ex. C:\\Users\\username\\AppData\\Roaming\\.minecraft\\saves\\world): ")

    folder_name = folder_name.strip('\"').strip('\'')

    base_folder_name = os.path.basename(folder_name)

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

    block_id_counter = 0 #theyre accessed just within a function that is called later
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

    #cache last block in event we run thru many blocks of same type
    #theyre accessed just within a function that is called later
    if custom: 
        last_key = (None, None, None)  # (base_name, prop_key_str or None, namespace or None)
        last_ids = (None, None, None) # (indexBlock, indexProp or None, indexNamespace or None)
    else: 
        last_key = (None, None)  # (base_name, prop_key_str or None)
        last_ids = (None, None) # (indexBlock, indexProp or None)

    # micro-aliases to speed dictionary ops

    b_id_get = block_id_dict.get

    b_prop_get = block_property_dict.get

    b_ns_get = block_namespace_dict.get 

    range_local_x = range(32)
    range_local_z = range(32)

    #version is above 1.18 or below 1.17
    new_or_old = None

    #check first section to get version of stored properties:

    for mca_file in mca_dir_list: # MCA scope
        if new_or_old is not None:
                    break
        
        region = anvil.Region.from_file(folder_name + "\\" + mca_file)
        
        region_x, region_z = map(int, mca_file.split(".")[1:3])

        region = anvil.Region.from_file(folder_name + "\\" + mca_file)
            
        region_x, region_z = map(int, mca_file.split(".")[1:3])
    
        for local_x in range_local_x:
                
                if new_or_old is not None:
                    break

                for local_z in range_local_z: #chunk scope

                    if new_or_old is not None:
                        break

                    if region.chunk_location(local_x, local_z) is None:
                        continue
                    try:
                        chunk = region.get_chunk(local_x, local_z)

                        for section_y in range(0 ,15):
                            section = chunk.get_section(section_y)

                            if not section:
                                continue

                            #1.18+ format:
                            if 'block_states' in section and 'palette' in section['block_states']:
                                new_or_old = True
                            
                            #1.17- format:
                            elif 'Palette' in section:
                                new_or_old = False
                            
                            break
                    except Exception:
                            continue
                    
    # lambda-ing boolean expressions that can be evaluated before the loop starts
     #1.18+ format:
    def p_1_18 (): 
        palette = section['block_states']['palette']
        if len(palette) == 1 and palette[0]['Name'].value.endswith('air'):
            return True
        else:
            return False
    
    #1.17- format:
    def p_1_17 ():
        palette = section['Palette']
        if len(palette) == 1 and palette[0]['Name'].value.endswith('air'):
            return True
        else:
            return False

    is_air_checker = p_1_18 if new_or_old else p_1_17 #check if section is fully air or not

    def cache_block_custom():
        nonlocal last_key, last_ids
        nonlocal block_id_counter, block_property_counter, block_namespace_counter
        namespace = block.namespace

        key = (base_name, properties, namespace)
        
        if key == last_key:
            indexBlock, indexProp, indexNamespace = last_ids
            return False
        else:
            indexProp = None
            indexNamespace = b_ns_get(namespace)

            indexBlock = b_id_get(base_name)

            if indexBlock is None:
                block_id_dict[base_name] = indexBlock = block_id_counter
                block_id_counter += 1

            if properties:
                indexProp = b_prop_get(properties)
                
                if indexProp is None:
                    block_property_dict[properties] = indexProp = block_property_counter
                    block_property_counter += 1

            if indexNamespace is None:
                block_namespace_dict[namespace] = indexNamespace = block_namespace_counter
                block_namespace_counter += 1

        last_ids = (indexBlock, indexProp, indexNamespace)
        last_key = key
        return True

    def cache_block_non_custom():
        nonlocal last_key, last_ids
        nonlocal block_id_counter, block_property_counter

        key = (base_name, properties)
                            
        if key == last_key:
            indexBlock, indexProp = last_ids
            return False
        else:
            indexProp = None

            indexBlock = b_id_get(base_name)

            if indexBlock is None:
                block_id_dict[base_name] = indexBlock = block_id_counter
                block_id_counter += 1

            if properties:
                indexProp = b_prop_get(properties)
                
                if indexProp is None:
                    block_property_dict[properties] = indexProp = block_property_counter
                    block_property_counter += 1

        last_ids = (indexBlock, indexProp)
        last_key = key
        return True
    
    cache_block = cache_block_custom if custom else cache_block_non_custom

    for mca_file in mca_dir_list: # MCA scope
        region = anvil.Region.from_file(folder_name + "\\" + mca_file)
        
        region_x, region_z = map(int, mca_file.split(".")[1:3])

        for local_x in range_local_x:
            for local_z in range_local_z: #chunk scope
                n+=1

                cx = 32 * region_x + local_x
                cz = 32 * region_z + local_z

                if not scan_all:
                    #chunks outside boundary
                    if not(cx*16 + 15 >= lowerX_lim and cx*16 <= upperX_lim and cz*16 + 15 >= lowerZ_lim and cz*16 <= upperZ_lim):
                        continue
                    #border chunks
                    else:               
                        if cx*16 + 15 > upperX_lim:
                            upperX = upperX_lim - 16*cx
                        else:
                            upperX = 16

                        if cx*16 < lowerX_lim:
                            lowerX= lowerX_lim - 16*cx
                        else:
                            lowerX = 0

                        if cz*16 + 15 > upperZ_lim:
                            upperZ = upperZ_lim - 16*cz
                        else:
                            upperZ = 16

                        if cz*16 < lowerZ_lim:
                            lowerZ = lowerZ_lim - 16*cz
                        else:
                            lowerZ = 0
                else:
                    lowerZ, upperZ = 0, 16
                    lowerX, upperX = 0, 16

                if region.chunk_location(local_x, local_z) is None:
                    continue
                
                chunk = region.get_chunk(local_x, local_z)

                existing_sections = []

                for section_y in range(lower_lim // 16, (upper_lim - 1) // 16 + 1):
                    section = chunk.get_section(section_y)

                    if not section:
                        continue

                    if not is_air_checker():
                        existing_sections.append(section_y)
                
                if not existing_sections:
                    continue

                stream = chunk.stream_chunk()
                for idx, block in enumerate(stream): #go thru this section

                    #note that the index is arranged with 3d->1d address logic: index = 16*16*y + 16*z + x
                    base_name = block.id

                    if base_name.endswith('air'):
                        continue 
                    
                    temp = idx // 16

                    y = temp // 16

                    if y < lower_lim or y >= upper_lim:
                        continue

                    Ox = idx % 16

                    if Ox < lowerX or Ox >= upperX:
                        continue

                    Oz = (temp) % 16

                    if Oz < lowerZ or Oz >= upperZ:
                            continue
                    
                    properties = block.properties

                    if properties:
                        #put off for later to be parsed into string (its a tuple of non-string tags)
                        properties = ",".join(f"{k}={v}" for k, v in sorted(properties.items()))
                    else: 
                        properties = None

                    if not cache_block(): #call our cache block function (it also updates all dics)
                        continue     

            # every ~32 chunks update the user on progress:
            curr_time = time.time()
            passed = curr_time - last_time
            print(f"chunk {round(n/total_chunks*100, 2)}% complete ({n}/{total_chunks}) - took {passed:.2f} seconds")
            last_time = curr_time  


    print("Creating BlockID key file\n")
    g = open(f"{base_folder_name}.blockIds.txt", "w")
    for key, value in block_id_dict.items():
        g.write(f"{key}={value}\n")
    g.close()        

    print("Creating BlockProperties key file\n")
    h = open(f"{base_folder_name}.blockProperties.txt", "w")
    for key, value in block_property_dict.items():
        h.write(f"{key}={value}\n")
    h.close()

    if custom:
        print("Creating BlockNamespaces key file\n")
        k = open(f"{base_folder_name}.blockNamespaces.txt", "w")
        for key, value in block_namespace_dict.items():
            k.write(f"{key}={value}\n")
        k.close()

    input("COMPLETE! Press <enter> to close...")

    exit(0)

                          
if __name__ == '__main__':
    main()
