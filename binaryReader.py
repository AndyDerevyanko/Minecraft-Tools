
def setUpInternalParams(folder_name, world_name, custom):
    #load up parameters corresponding to file organization of world
    #we only read first 4 or 5 (custom) lines so we can get the header parameters then open in binary

    world = open(folder_name + f"\\{world_name}", "r")

    grab_next = lambda: int(world.readline().split(":")[1].removesuffix("\n"))

    if custom:
        NS_BYTES = grab_next()

    ID_PROP_BYTES = grab_next()
    X_COORD_BYTES = grab_next()
    Y_COORD_BYTES = grab_next()
    Z_COORD_BYTES = grab_next()

    world.close() #close file
    
    NUM_COORD_BYTES = X_COORD_BYTES + Y_COORD_BYTES + Z_COORD_BYTES

    #now we read the file
    if custom:
        BLOCK_SIZE = (
        NS_BYTES+
        2 * ID_PROP_BYTES
        + NUM_COORD_BYTES)
    else:
        BLOCK_SIZE = (
        2 * ID_PROP_BYTES
        + NUM_COORD_BYTES)

    CHUNK = BLOCK_SIZE * 4096

    if custom: 
        #the offset we have to add to each block to get its corresponding property idx
        X_COORD_INDEX = NS_BYTES + 2*ID_PROP_BYTES
        Y_COORD_INDEX = X_COORD_INDEX + X_COORD_BYTES
        Z_COORD_INDEX = Y_COORD_INDEX + Y_COORD_BYTES
        END_INDEX = BLOCK_SIZE

        return((X_COORD_INDEX, Y_COORD_INDEX, Z_COORD_INDEX, END_INDEX), 
               (BLOCK_SIZE, CHUNK, X_COORD_BYTES, Y_COORD_BYTES, Z_COORD_BYTES)
               )
    else:
        #the offset we have to add to each block to get its corresponding property idx
        X_COORD_INDEX = ID_PROP_BYTES*2
        Y_COORD_INDEX = X_COORD_INDEX + X_COORD_BYTES
        Z_COORD_INDEX = Y_COORD_INDEX + Y_COORD_BYTES
        END_INDEX = BLOCK_SIZE

        return((X_COORD_INDEX, Y_COORD_INDEX, Z_COORD_INDEX, END_INDEX), 
               (BLOCK_SIZE, CHUNK, X_COORD_BYTES, Y_COORD_BYTES, Z_COORD_BYTES)
               )

def getBlockCoords(blocks, block_idx, INDICES, BYTES):
            
            X_COORD_INDEX, Y_COORD_INDEX, Z_COORD_INDEX, END_INDEX = INDICES
            X_COORD_BYTES, Y_COORD_BYTES, Z_COORD_BYTES = BYTES
                                
            t_x_coord_index = X_COORD_INDEX + block_idx
            t_y_coord_index = Y_COORD_INDEX + block_idx
            t_z_coord_index = Z_COORD_INDEX + block_idx
            t_end_index = END_INDEX + block_idx

            #coords must be converted according to sign bit
            raw_x = int.from_bytes(blocks[t_x_coord_index:t_y_coord_index], "little", signed=False)
            raw_x_length = X_COORD_BYTES*8

            x_sign = bool(raw_x & (1 << (raw_x_length - 1)))

            t_x_coord = raw_x & ~(1 << (raw_x_length - 1))
            t_x_coord = -t_x_coord if x_sign else t_x_coord

            #y-sign
            raw_y = int.from_bytes(blocks[t_y_coord_index:t_z_coord_index], "little", signed=False)
            raw_y_length = Y_COORD_BYTES*8

            y_sign = bool(raw_y & (1 << (raw_y_length - 1)))

            t_y_coord = raw_y & ~(1 << (raw_y_length - 1))
            t_y_coord = -t_y_coord if y_sign else t_y_coord

            #z-sign
            raw_z = int.from_bytes(blocks[t_z_coord_index:t_end_index], "little", signed=False)
            raw_z_length = Z_COORD_BYTES*8

            z_sign = bool(raw_z & (1 << (raw_z_length - 1)))

            t_z_coord = raw_z & ~(1 << (raw_z_length - 1))
            t_z_coord = -t_z_coord if z_sign else t_z_coord

            return (t_x_coord, t_y_coord, t_z_coord)

def main():
    folder_name = input("Input folder where dictionaries and world file are stored: ")
    world_name = input("Enter world name: ")

    custom = True if input("Does your world have any custom (modded) blocks? : ").lower()== "y" else False
    need_properties = True if input("Do you want to analyze property dictionary too? : ").lower()== "y" else False
    need_search = True if input("Y = search the file for a specific block name OR N = just output all blocks in the world sequentially: ").lower()== "y" else False

    if need_search:
        search_id = input(f"Enter the block name (String, minecraft block_id, can be found in {world_name}.blockIDs): ").lower().strip('\"').strip('\'').strip()


    folder_name = folder_name.strip('\"').strip('\'').rstrip('\\')

    #open all files
    ids = open(folder_name + f"\\{world_name}.blockIds", "r")
    props = open(folder_name + f"\\{world_name}.blockProperties", "r")
    world = open(folder_name + f"\\{world_name}.world", "r")

    ids_dict = {}
    props_dict = {}

    if custom:
        ns = open(folder_name + f"\\{world_name}.blockNamespaces", "r")
        ns_dict = {}

        #load up ns if custom is true
        for line in ns:
            segs = line.partition("=")
            ns_dict[int(segs[0])] = segs[2].strip()

    #load up all dictionaries
    for line in ids:
        segs = line.partition("=")
        ids_dict[int(segs[0])] = segs[2].strip()

    for line in props:
        segs = line.partition("=")
        props_dict[int(segs[0])] = segs[2].strip()


    #load up parameters corresponding to file organization of world
    #we only read first 4 or 5 (custom) lines so we can get the header parameters then open in binary

    grab_next = lambda: int(world.readline().split(":")[1].removesuffix("\n"))

    if custom:
        NS_BYTES = grab_next()

    ID_PROP_BYTES = grab_next()
    X_COORD_BYTES = grab_next()
    Y_COORD_BYTES = grab_next()
    Z_COORD_BYTES = grab_next()

    world.close()
    world = open(folder_name + f"\\{world_name}.world", "rb")

    for _ in range(4 + int(custom)): #go over first 5 lines if custom, else 4
        world.readline()

    NUM_COORD_BYTES = X_COORD_BYTES + Y_COORD_BYTES + Z_COORD_BYTES

    #now we read the file
    if custom:
        BLOCK_SIZE = (
        NS_BYTES+
        2 * ID_PROP_BYTES
        + NUM_COORD_BYTES)
    else:
        BLOCK_SIZE = (
        2 * ID_PROP_BYTES
        + NUM_COORD_BYTES)

    CHUNK = BLOCK_SIZE * 4096

    if custom: 
        #the offset we have to add to each block to get its corresponding property idx
        NAMESPACE_INDEX = 0
        IDS_INDEX = NS_BYTES
        PROPS_INDEX = IDS_INDEX + ID_PROP_BYTES
        X_COORD_INDEX = PROPS_INDEX + ID_PROP_BYTES
        Y_COORD_INDEX = X_COORD_INDEX + X_COORD_BYTES
        Z_COORD_INDEX = Y_COORD_INDEX + Y_COORD_BYTES
        END_INDEX = BLOCK_SIZE
    else:
        #the offset we have to add to each block to get its corresponding property idx
        IDS_INDEX = 0
        PROPS_INDEX = ID_PROP_BYTES
        X_COORD_INDEX = PROPS_INDEX + ID_PROP_BYTES
        Y_COORD_INDEX = X_COORD_INDEX + X_COORD_BYTES
        Z_COORD_INDEX = Y_COORD_INDEX + Y_COORD_BYTES
        END_INDEX = BLOCK_SIZE


    #handle search if requested, otherwise loop thru all blocks
    if need_search: 
        raw_id = -1
        for k, v in ids_dict.items():
            if v == search_id:
                raw_id = k
                break

        if raw_id == -1:
            print("ERROR: the block you have requested to search for has not been found, exiting...")
            exit(1)

        def goThroughThisBlock():
            t_ids_index = IDS_INDEX + block_idx
            t_props_index = PROPS_INDEX + block_idx
            t_x_coord_index = X_COORD_INDEX + block_idx
            t_y_coord_index = Y_COORD_INDEX + block_idx
            t_z_coord_index = Z_COORD_INDEX + block_idx
            t_end_index = END_INDEX + block_idx

            if custom:
                t_namespace_index = NAMESPACE_INDEX + block_idx
                t_namespace = ns_dict[int.from_bytes(blocks[t_namespace_index:t_ids_index], "little", signed=False)]

            if need_properties:
                t_props = props_dict[int.from_bytes(blocks[t_props_index:t_x_coord_index], "little", signed=False)]


            t_id = ids_dict[int.from_bytes(blocks[t_ids_index:t_props_index], "little", signed=False)]

            #coords must be converted according to sign bit
            raw_x = int.from_bytes(blocks[t_x_coord_index:t_y_coord_index], "little", signed=False)
            raw_x_length = X_COORD_BYTES*8

            x_sign = bool(raw_x & (1 << (raw_x_length - 1)))

            t_x_coord = raw_x & ~(1 << (raw_x_length - 1))
            t_x_coord = -t_x_coord if x_sign else t_x_coord

            #y-sign
            raw_y = int.from_bytes(blocks[t_y_coord_index:t_z_coord_index], "little", signed=False)
            raw_y_length = Y_COORD_BYTES*8

            y_sign = bool(raw_y & (1 << (raw_y_length - 1)))

            t_y_coord = raw_y & ~(1 << (raw_y_length - 1))
            t_y_coord = -t_y_coord if y_sign else t_y_coord

            #z-sign
            raw_z = int.from_bytes(blocks[t_z_coord_index:t_end_index], "little", signed=False)
            raw_z_length = Z_COORD_BYTES*8

            z_sign = bool(raw_z & (1 << (raw_z_length - 1)))

            t_z_coord = raw_z & ~(1 << (raw_z_length - 1))
            t_z_coord = -t_z_coord if z_sign else t_z_coord

            if custom and need_properties:
                print(f"NS:{t_namespace}, BLK:{t_id}, PROP:{t_props}, COORDS:{t_x_coord},{t_y_coord},{t_z_coord}")
            elif custom:
                print(f"NS:{t_namespace}, BLK:{t_id}, COORDS:{t_x_coord},{t_y_coord},{t_z_coord}")
            elif need_properties:
                print(f"BLK:{t_id}, PROP:{t_props}, COORDS:{t_x_coord},{t_y_coord},{t_z_coord}")
            else:
                print(f"BLK:{t_id}, COORDS:{t_x_coord},{t_y_coord},{t_z_coord}")

        while True:
            blocks = world.read(CHUNK*64) #read 64 chunks at a time

            if not blocks:
                break
            
            for block_idx in range(0, len(blocks), BLOCK_SIZE):
                t_ids_index = IDS_INDEX + block_idx
                t_props_index = PROPS_INDEX + block_idx

                t_id = int.from_bytes(blocks[t_ids_index:t_props_index], "little", signed=False)

                if t_id == raw_id:
                    goThroughThisBlock()

        print("COMPLETED SEARCH, if output is blank, nothing was found (potential world corruption)")
        

    else:
        while True:
            blocks = world.read(CHUNK*64) #read 64 chunks at a time

            if not blocks:
                break

            for block_idx in range(0, len(blocks), BLOCK_SIZE):
                goThroughThisBlock()
        
        print("----------\nCOMPLETED")
    

    world.close()
    ids.close()
    props.close()
    
    if custom:
        ns.close()
    

if __name__ == '__main__':
    main()