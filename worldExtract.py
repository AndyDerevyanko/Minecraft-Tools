
import anvil
import time
import os
import tempfile

from multiprocessing import Process, cpu_count, Queue

def process_core(k, core_count, mca_list_length, output_queue, config):
    total_chunks = 0

    range_local_x = range(32)
    range_local_z = range(32)

    #unpack the configuration variables passed from main
    folder_name = config['folder_name']
    mca_dir_list = config['mca_dir_list']
    scan_all = config['scan_all']
    lower_lim = config['lower_lim']
    upper_lim = config['upper_lim']
    custom = config['custom']

    num_coords_bytes_y = config['num_coords_bytes_y']
    num_coords_bytes_x = config['num_coords_bytes_x']
    num_coords_bytes_z = config['num_coords_bytes_z']
    num_blocks_props_bytes = config['num_blocks_props_bytes']

    #if scan_all is false, we need these:
    lowerX_lim = config.get('lowerX_lim')
    upperX_lim = config.get('upperX_lim')
    lowerZ_lim = config.get('lowerZ_lim')
    upperZ_lim = config.get('upperZ_lim')
    num_namespaces_bytes = config.get('num_namespaces_bytes')

    #will write to this bytes file
    f = open(f"process_{k}", "wb")
    start_time = time.time()

    #get chunk count for this core
    for mca_index in range(k, mca_list_length, core_count): # MCA scope
        mca_file = mca_dir_list[mca_index]
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

    # create dictionaries for each core
    block_id_dict= {}
    block_property_dict= {None: 0} #reserve 0 for empty properties

    if custom:
        block_namespace_dict = {}

    block_id_counter = 0
    block_property_counter = 1 #for that blank one
    block_namespace_counter = 0

    output_lines = []

    #cache last block in event we run thru many blocks of same type
    if custom: 
        last_key = (None, None, None)  # (namespace or None, base_name, prop_key_str or None, )
        last_ids = (None, None, None) # (indexNamespace or None, indexBlock, indexProp or None, )
    else: 
        last_key = (None, None)  # (base_name, prop_key_str or None)
        last_ids = (None, None) # (indexBlock, indexProp or None)

    # micro-aliases to speed dictionary ops
    b_id_get = block_id_dict.get

    b_prop_get = block_property_dict.get

    if custom:
        b_ns_get = block_namespace_dict.get 

    n = 0 #counter for how many chunks we've gone thru
    last_time = time.time()
                    
    for mca_index in range(k, mca_list_length, core_count): # MCA scope
        mca_file = mca_dir_list[mca_index]
        region = anvil.Region.from_file(folder_name + "\\" + mca_file)
        
        region_x, region_z = map(int, mca_file.split(".")[1:3])

        for local_x in range_local_x:
            for local_z in range_local_z: #chunk scope

                cx = 32 * region_x + local_x
                cz = 32 * region_z + local_z

                if not scan_all:
                    #chunks outside boundary
                    if not(cx*16 + 15 >= lowerX_lim and cx*16 <= upperX_lim and cz*16 + 15 >= lowerZ_lim and cz*16 <= upperZ_lim):
                        continue
                    #border chunks
            
                    upperX = upperX_lim - 16*cx if cx*16 + 15 > upperX_lim else 16

                    lowerX = lowerX_lim - 16*cx if cx*16 < lowerX_lim else 0

                    upperZ = upperZ_lim - 16*cz if cz*16 + 15 > upperZ_lim else 16

                    lowerZ = lowerZ_lim - 16*cz if cz*16 < lowerZ_lim else 0
                        
                else:
                    lowerZ, upperZ = 0, 16
                    lowerX, upperX = 0, 16

                #increment here as chunk has been "noticed", even if it is invalid: it was part of total_chunk counter
                n+=1
                
                #all cases of errors
                try: 
                    if region.chunk_location(local_x, local_z) is None:
                        continue
                except Exception:
                    continue

                try:
                    chunk = region.get_chunk(local_x, local_z)
                except Exception:
                    continue
                
                #go per section, skip sections of only air (optimization)
                #NOTE: upper_lim-1 because we doing exclusive, +1 after becuase range(...) is exclusive
                for section_y in range(lower_lim // 16, (upper_lim - 1) // 16 + 1):
                    section = chunk.get_section(section_y)

                    if not section:
                        continue
                    
                    #1.18+ format:
                    if 'block_states' in section and 'palette' in section['block_states']:
                        palette = section['block_states']['palette']
                        if len(palette) == 1 and palette[0]['Name'].value.endswith('air'):
                            continue #is air only
                    
                    #1.17- format:
                    elif 'Palette' in section: 
                        palette = section['Palette']
                        if len(palette) == 1 and palette[0]['Name'].value.endswith('air'):
                            continue #is air only
                    
                    #find the starting block index for this vertical section (16×16×16 = 256 blocks per Y-layer)
                    #stream_blocks needs a base index so it can map the block stream to world Y coordinates correctly
                    base_index = section_y * 256

                    #properly grab chunk to stream blocks
                    
                    stream = chunk.stream_blocks(section=section_y)
                    for idx, block in enumerate(stream):
                        base_name = block.id

                        if base_name.endswith('air'):
                            continue

                        y = (idx >> 8 & 0xF) + (16 * section_y)

                        if y < lower_lim or y >= upper_lim:
                            continue

                        #if we go through all chunks, we do not need to check lims (on z and x)
                        if not scan_all:
                            Ox = idx & 0xF

                            if Ox < lowerX or Ox >= upperX:
                                continue

                            Oz = idx >> 4 & 0xF

                            if Oz < lowerZ or Oz >= upperZ:
                                    continue
                        else:
                            Ox = idx & 0xF

                            Oz = idx >> 4 & 0xF
                        

                        properties = block.properties

                        if properties:
                            #put off for later to be parsed into string (its a tuple of non-string tags)
                            properties = ",".join(f"{keyp}={valuep}" for keyp, valuep in properties.items())
                        else: 
                            properties = None
                        
                        #calculate x and z 
                        coordX = Ox + cx*16
                        coordZ = Oz + cz*16
                        
                        #get x,y,z coords neg component if it exists
                        #NOTE: we use 1 bit for sign (NOT 2's complement)
                        if coordX < 0:
                            coordX = abs(coordX) | (0b1 << (num_coords_bytes_x*8 - 1))
                                                    
                        if coordZ < 0:
                            coordZ = abs(coordZ) | (0b1 << (num_coords_bytes_z*8 - 1))   

                        if y < 0:
                            y = abs(y) | (0b1 << (num_coords_bytes_y*8 - 1))

                        #the time-saver using cached block
                        if custom: 
                            namespace = block.namespace
                            key = (namespace, base_name, properties)

                            if last_key == key:
                                output_lines.extend(last_ids)
                                output_lines.extend((coordX, y, coordZ))

                            else:
                                indexNamespace = b_ns_get(namespace)
                                indexBlock = b_id_get(base_name)

                                if indexBlock is None:
                                    block_id_dict[base_name] = indexBlock = block_id_counter
                                    block_id_counter += 1


                                if indexNamespace is None:
                                        block_namespace_dict[namespace] = indexNamespace = block_namespace_counter
                                        block_namespace_counter += 1

                                if properties:
                                    indexProp = b_prop_get(properties)
                                    
                                    if indexProp is None:
                                        block_property_dict[properties] = indexProp = block_property_counter
                                        block_property_counter += 1

                                    output_lines.extend((indexNamespace, indexBlock, indexProp, coordX, y, coordZ))
                                    last_ids = (indexNamespace, indexBlock, indexProp)
                                else:
                                    output_lines.extend((indexNamespace, indexBlock, 0, coordX, y, coordZ))
                                    last_ids = (indexNamespace, indexBlock, 0)

                        else:
                            key = (base_name, properties)

                            if last_key == key:
                                output_lines.extend(last_ids)
                                output_lines.extend((coordX, y, coordZ))

                            else:
                                indexBlock = b_id_get(base_name)

                                if indexBlock is None:
                                    block_id_dict[base_name] = indexBlock = block_id_counter
                                    block_id_counter += 1

                                if properties:
                                    indexProp = b_prop_get(properties)
                                    
                                    if indexProp is None:
                                        block_property_dict[properties] = indexProp = block_property_counter
                                        block_property_counter += 1
                                    
                                    output_lines.extend((indexBlock, indexProp, coordX, y, coordZ))
                                    last_ids = (indexBlock, indexProp)
                                    
                                else:
                                    output_lines.extend((indexBlock, 0, coordX, y, coordZ))   
                                    last_ids = (indexBlock, 0)

                        last_key = key

            #print progress roughly 1/10 times every 32 chunks
            if n % 10 == 0:
                curr_time = time.time()
                passed = curr_time - last_time
                if total_chunks:
                    print(f"CORE {k + 1}: chunk {n/total_chunks*100:.2f}% complete ({n}/{total_chunks}) - took {passed:.2f} seconds")
                last_time = curr_time
            
            #flush output stream 1/10 times every 32 chunks
                if custom:
                    elem_n = 0
                    while True:
                        if elem_n >= len(output_lines):
                            break

                        f.write(output_lines[elem_n].to_bytes(num_namespaces_bytes, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_blocks_props_bytes, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_blocks_props_bytes, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_coords_bytes_x, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_coords_bytes_y, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_coords_bytes_z, "little"))
                        elem_n+=1
                else: 
                    elem_n = 0
                    while True:
                        if elem_n >= len(output_lines):
                            break
                
                        f.write(output_lines[elem_n].to_bytes(num_blocks_props_bytes, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_blocks_props_bytes, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_coords_bytes_x, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_coords_bytes_y, "little"))
                        elem_n+=1
                        f.write(output_lines[elem_n].to_bytes(num_coords_bytes_z, "little"))
                        elem_n+=1
            
                output_lines.clear()

    if custom:
        #final flush of any remaining blocks
        elem_n = 0
        while elem_n < len(output_lines):
            f.write(output_lines[elem_n].to_bytes(num_namespaces_bytes, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_blocks_props_bytes, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_blocks_props_bytes, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_coords_bytes_x, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_coords_bytes_y, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_coords_bytes_z, "little"))
            elem_n+=1

        output_queue.put((k, block_id_dict, block_property_dict, block_namespace_dict))
    else: 
        #final flush of any remaining blocks
        elem_n = 0
        while elem_n < len(output_lines):
            f.write(output_lines[elem_n].to_bytes(num_blocks_props_bytes, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_blocks_props_bytes, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_coords_bytes_x, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_coords_bytes_y, "little"))
            elem_n+=1
            f.write(output_lines[elem_n].to_bytes(num_coords_bytes_z, "little"))
            elem_n+=1

        output_queue.put((k, block_id_dict, block_property_dict))

    f.close()  
    print(f"***CORE {k + 1}: chunk 100.00% complete ({total_chunks}/{total_chunks}) - took {time.strftime('%H:%M:%S',time.gmtime(time.time() - start_time))}")
    

def main():
    import ctypes

    kernel32 = ctypes.windll.kernel32
    handle = kernel32.GetStdHandle(-10)
    mode = ctypes.c_ulong()
    kernel32.GetConsoleMode(handle, ctypes.byref(mode))
    kernel32.SetConsoleMode(handle, (mode.value & ~0x0040) | 0x0080) #disable quick edit, keep all other input flags

    folder_name = input("Input World Path (ex. C:\\Users\\username\\AppData\\Roaming\\.minecraft\\saves\\world): ")

    folder_name = folder_name.strip().strip('\"').strip('\'').rstrip('\\').strip()

    base_folder_name = os.path.basename(folder_name)

    dimension = input("Enter dimension folder (if NOT custom (modded) dimension, use:  overworld: \'.\\\', nether: \'DIM-1\', end: \'DIM1\'): ").lower()

    dimension = dimension.strip().strip('\"').strip('\'').strip('\\').strip('/').strip()

    if dimension == ".":
        folder_name+= ("\\region")
    else:
        folder_name+= ("\\" + dimension + "\\region")

    mca_dir_list = [f for f in os.listdir(folder_name) if f.lower().endswith('.mca')] #make sure we use only mca files

    upper_lim = int(input("Enter the max height to which should be scanned (Usually 256 or 320 depending on version) (exclusive): "))

    lower_lim = int(input("Enter the min height to which should be scanned (Usually 0 or -64 depending on version) (inclusive): "))

    scan_all = True if input("Scan entire world or define Horizontal limits? (Y: Scan whole world, N: Define limits): ").lower() == "y" else False

    if not scan_all:
        lowerX_lim = int(input("Enter the min X (inclusive): "))
        upperX_lim = int(input("Enter the max X (inclusive): "))
        lowerZ_lim = int(input("Enter the min Z (inclusive): "))
        upperZ_lim = int(input("Enter the max Z (inclusive): "))

    custom = True if input("Are there custom (modded) blocks in your world? (Y/N): ").lower() == "y" else False
    
    #give this one a "sign" bit that will be used to determine whether empty or not
    #NOTE: NUM_BLOCKS_PROPS_BYTES IS USED FOR BOTH PROPERTIES AND BLOCKS
    num_blocks_props_bytes = (((int(input("Since there are custom blocks, this program requires you to input the number of blocks present in your modded world (or instance of minecraft). This can be found through a variety of methods, including using analytics mods. NOTE: vanilla minecraft has around 890"))+1).bit_length()+7)//8) if custom else 2

    num_coords_bytes_y = (max(abs(upper_lim), abs(lower_lim)).bit_length() + 8)//8 # 1 extra bit for sign

    if not scan_all:
        num_coords_bytes_x = (max(abs(lowerX_lim), abs(upperX_lim)).bit_length() + 8)//8 
        num_coords_bytes_z = (max(abs(lowerZ_lim), abs(upperZ_lim)).bit_length() + 8)//8
    else:
       num_coords_bytes_x = 4 
       num_coords_bytes_z = 4 
    
    
    guard_zero = lambda var: 1 if not var else var

    #allocate minimum 1 byte 
    num_coords_bytes_y = guard_zero(num_coords_bytes_y)
    num_coords_bytes_x = guard_zero(num_coords_bytes_x)
    num_coords_bytes_z = guard_zero(num_coords_bytes_z)
    num_blocks_props_bytes = guard_zero(num_blocks_props_bytes)

    if custom:
        num_namespaces_bytes = 1 # if custom, we need to consider namespace

    #divide mca_files across each core
    config = {
        'folder_name': folder_name,
        'mca_dir_list': mca_dir_list,
        'scan_all': scan_all,
        'lower_lim': lower_lim,
        'upper_lim': upper_lim,
        'custom': custom,
        'lowerX_lim': lowerX_lim if not scan_all else None,
        'upperX_lim': upperX_lim if not scan_all else None,
        'lowerZ_lim': lowerZ_lim if not scan_all else None,
        'upperZ_lim': upperZ_lim if not scan_all else None,
        'num_blocks_props_bytes': num_blocks_props_bytes,
        'num_coords_bytes_x': num_coords_bytes_x,
        'num_coords_bytes_y': num_coords_bytes_y,
        'num_coords_bytes_z': num_coords_bytes_z,
        'num_namespaces_bytes': num_namespaces_bytes if custom else None
    }

    #create queue for outputs of multiprocessing
    results_queue = Queue()
    mca_list_length = len(mca_dir_list)
    core_count = cpu_count()

    print("Starting block processing, this IS THE MOST TIME CONSUMING STAGE BY FAR: Stage 1/6")
    processes = [Process(target=process_core, args=(core_number, core_count, mca_list_length, results_queue, config)) for core_number in range(core_count)]
    
    for process in processes:
        process.start()

    #create master and local process storage containers (for all processes' outputs)
    #these containers start off as sets (to merge)
    master_ids = set()
    master_props = set()

    #list to store every dictionary BUT REVERSED, because we need to dereference by number within files when syncing local->global block ids, props, and ns
    #NOTE: the "dictionaries" become lists, as the dereferencing of the individual index gives the 
    local_id_r_dicts = [None] * core_count 
    local_props_r_dicts = [None] * core_count
    

    if custom:
        master_ns = set()
        local_ns_r_dicts = [None] * core_count

    #collect all keys and dictionaries for merging 
    #NOTE: order of processes does not matter anymore, they all get merged the same
    for _ in range(len(processes)):
        if custom:
            core_k, core_ids, core_props, core_ns = results_queue.get()
        else: 
            core_k, core_ids, core_props = results_queue.get()

        #updated "REVERSED" dictionaries for each process
        id_r = ([None] * len(core_ids))
        props_r = ([None] * len(core_props))

        if custom:
            ns_r = ([None] * len(core_ns))

            #construct namespace list if custom
            for c_key, c_num in core_ns.items():
                ns_r[c_num] = c_key
            
            local_ns_r_dicts[core_k] = ns_r

        #construct that list
        for c_key, c_num in core_ids.items():
            id_r[c_num] = c_key

        for c_key, c_num in core_props.items():
            props_r[c_num] = c_key

        #map process to proper index
        local_id_r_dicts[core_k] = id_r
        local_props_r_dicts[core_k] = props_r

        if custom:
            master_ns.update(core_ns.keys())

        master_ids.update(core_ids.keys())
        master_props.update(core_props.keys())
        
    #clear the last reference for deletion
    core_ids.clear()
    core_props.clear()

    if custom:
        core_ns.clear()

    for process in processes:
        process.join()

    print("DONE COLLECTING BLOCKS: Merging dictionaries from each core: Stage 2/6")

    #enumerate all values to make master dictionaries
    #NOTE we delete sets as we go as they are a waste of space
    master_id_dict = {string: idx for idx, string in enumerate(master_ids)}
    master_ids.clear()

    master_props_dict = {string: idx for idx, string in enumerate(master_props)}
    master_props.clear()

    if custom:
        master_ns_dict = {string: idx for idx, string in enumerate(master_ns)}
        master_ns.clear()

    print("Now remapping files: Stage 3/6")
    #now, we can do file remapping
    if custom:
        num_coords_bytes = num_coords_bytes_x  + num_coords_bytes_y + num_coords_bytes_z

        BLOCK_SIZE = (
        num_namespaces_bytes
        + 2 * num_blocks_props_bytes
        + num_coords_bytes)    

        CHUNK = BLOCK_SIZE * 4096

        #the offset we have to add to each block to get its corresponding property idx
        NAMESPACE_INDEX = 0
        IDS_INDEX = num_namespaces_bytes
        PROPS_INDEX = IDS_INDEX + num_blocks_props_bytes
        END_ELEMENTS_INDEX = PROPS_INDEX + num_blocks_props_bytes

        #update everything: namespace number, property number, and 
        for k in range(core_count):
            start_time = time.time()
            src = f"process_{k}"

            # skip cores that processed nothing
            if os.path.getsize(src) == 0:
                continue

            #alias current dictionaries
            t_local_ns_r_dict = local_ns_r_dicts[k]
            t_local_id_r_dict = local_id_r_dicts[k]
            t_local_props_r_dict = local_props_r_dicts[k]
            
            with open(src, "r+b") as f: # open corresponding file
                while True:
                    #read 64 chunks at a time to save read/writes
                    seek_to = f.tell()
                    blocks = bytearray(f.read(64*CHUNK))

                    new_blocks = bytearray()
                    #new_blocks = bytearray(len(blocks))
                    #write_pos = 0

                    if not blocks:
                        break

                    for block_idx in range(0, len(blocks), BLOCK_SIZE):
                        #get local values of block
                        t_namespace_idx = block_idx + NAMESPACE_INDEX
                        t_ids_index = block_idx + IDS_INDEX
                        t_props_index = block_idx + PROPS_INDEX
                        t_block_elems_end = block_idx + END_ELEMENTS_INDEX
                        t_end = block_idx + BLOCK_SIZE
                        
                        #obtain master value of corresponding element by doing:
                        #get number from file -> get element string -> get master number (then insert later)
                        ns_bytes = master_ns_dict[t_local_ns_r_dict[int.from_bytes(blocks[t_namespace_idx:t_ids_index], "little", signed=False)]].to_bytes(num_namespaces_bytes, "little")
                        id_bytes = master_id_dict[t_local_id_r_dict[int.from_bytes(blocks[t_ids_index:t_props_index], "little", signed=False)]].to_bytes(num_blocks_props_bytes, "little")
                        props_bytes = master_props_dict[t_local_props_r_dict[int.from_bytes(blocks[t_props_index:t_block_elems_end], "little", signed=False)]].to_bytes(num_blocks_props_bytes, "little")
                        coord_bytes = blocks[t_block_elems_end:t_end]

                        new_blocks+=ns_bytes
                        #new_blocks[write_pos:write_pos + num_blocks_props_bytes] = ns_bytes
                        #write_pos += num_namespaces_bytes

                        new_blocks+=id_bytes
                        #new_blocks[write_pos:write_pos + num_blocks_props_bytes] = id_bytes
                        #write_pos += num_blocks_props_bytes

                        new_blocks+=props_bytes
                        #new_blocks[write_pos:write_pos + num_blocks_props_bytes] = props_bytes
                        #write_pos += num_blocks_props_bytes

                        new_blocks+=coord_bytes
                        #new_blocks[write_pos:write_pos + num_coords_bytes] = coord_bytes
                        #write_pos += num_coords_bytes

                    #go back to start of the data we modifying right now
                    f.seek(seek_to)

                    f.write(new_blocks) #write our new bytes object back in

            print(f"Remapped core {k+1}/{core_count} took: {(time.time() - start_time):.2f} seconds")

    else:
        num_coords_bytes = num_coords_bytes_x  + num_coords_bytes_y + num_coords_bytes_z

        BLOCK_SIZE = (
        2 * num_blocks_props_bytes
        + num_coords_bytes)    

        CHUNK = BLOCK_SIZE * 4096

        #the offset we have to add to each block to get its corresponding property idx
        IDS_INDEX = 0
        PROPS_INDEX = num_blocks_props_bytes
        END_ELEMENTS_INDEX = PROPS_INDEX + num_blocks_props_bytes

        #update everything: namespace number, property number, and 
        for k in range(core_count):
            start_time = time.time()
            print(f"Remapping core {k+1}/{core_count}")
            src = f"process_{k}"

            #alias current dictionaries
            t_local_id_r_dict = local_id_r_dicts[k]
            t_local_props_r_dict = local_props_r_dicts[k]
            
            with open(src, "r+b") as f: # open corresponding file
                while True:
                    #read 64 chunks at a time to save read/writes
                    seek_to = f.tell()
                    blocks = bytearray(f.read(64*CHUNK))

                    new_blocks = bytearray()
                    #new_blocks = bytearray(len(blocks))
                    #write_pos = 0

                    if not blocks:
                        break

                    for block_idx in range(0, len(blocks), BLOCK_SIZE):
                        #get local values of block
                        t_ids_index = block_idx + IDS_INDEX
                        t_props_index = block_idx + PROPS_INDEX
                        t_block_elems_end = block_idx + END_ELEMENTS_INDEX
                        t_end = block_idx + BLOCK_SIZE
                        
                        #obtain master value of corresponding element by doing:
                        #get number from file -> get element string -> get master number (then insert later)
                        id_bytes = master_id_dict[t_local_id_r_dict[int.from_bytes(blocks[t_ids_index:t_props_index], "little", signed=False)]].to_bytes(num_blocks_props_bytes, "little")
                        props_bytes = master_props_dict[t_local_props_r_dict[int.from_bytes(blocks[t_props_index:t_block_elems_end], "little", signed=False)]].to_bytes(num_blocks_props_bytes, "little")
                        coord_bytes = blocks[t_block_elems_end:t_end]

                        new_blocks+=id_bytes
                        #new_blocks[write_pos:write_pos + num_blocks_props_bytes] = id_bytes
                        #write_pos += num_blocks_props_bytes

                        new_blocks+=props_bytes
                        #new_blocks[write_pos:write_pos + num_blocks_props_bytes] = props_bytes
                        #write_pos += num_blocks_props_bytes

                        new_blocks+=coord_bytes
                        #new_blocks[write_pos:write_pos + num_coords_bytes] = coord_bytes
                        #write_pos += num_coords_bytes

                    #go back to start of the data we modifying right now
                    f.seek(seek_to)

                    f.write(new_blocks) #write our new bytes object back in

            print(f"Remapped core {k+1}/{core_count} took: {(time.time() - start_time):.2f} seconds")

    #Now, we just need to merge all the core files into one and make the master dictionary files
    print(f"Merging each cores' files into one big block file in: {os.getcwd()}, Stage 4/6\n")

    #now merge all files
    with open(f"{base_folder_name}.world", "wb") as block_file:
        if custom:
            block_file.write(f"namespace bytes: {num_namespaces_bytes}\n".encode())

        block_file.write(f"id/prop bytes: {num_blocks_props_bytes}\n".encode())
        block_file.write(f"x coord bytes: {num_coords_bytes_x}\n".encode())
        block_file.write(f"y coord bytes: {num_coords_bytes_y}\n".encode())
        block_file.write(f"z coord bytes: {num_coords_bytes_z}\n".encode())

        for k in range(len(processes)):
            f_name = f"process_{k}"

            if os.path.getsize(f_name) == 0:
                continue

            f = open(f_name, "rb")

            while True:
                blocks = f.read(CHUNK*64) #read 64 chunks at a time

                if not blocks:
                    break

                block_file.write(blocks)

            f.close()
            os.remove(f_name)

    #now handle all supplementaries
    print(f"Outputting all supplementaries: {os.getcwd()}, Stage 5/6\n")
    print(f"Creating BlockID key file in {os.getcwd()}\n")
    g = open(f"{base_folder_name}.blockIds", "w")
    for value, key in master_id_dict.items():
        g.write(f"{key}={value}\n")
    g.close()        

    print(f"Creating BlockProperties key file in {os.getcwd()}\n")
    h = open(f"{base_folder_name}.blockProperties", "w")
    for value, key in master_props_dict.items():
        h.write(f"{key}={value}\n")
    h.close()

    if custom:
        print(f"Creating BlockNamespaces key file in {os.getcwd()}\n")
        k = open(f"{base_folder_name}.blockNamespaces", "w")
        for value, key in master_ns_dict.items():
            k.write(f"{key}={value}\n")
        k.close()

    print("*******************COMPLETE!*******************")

                          
if __name__ == '__main__':
    main()