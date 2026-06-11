import time

class ChunkDoesNotExist(Exception):
    pass


coords = [0,0]

upper_lim = 4;
lower_lim = -4;

dirX = -1;
dirY = -1;

#test if main chunk exists
try:
    curr_block = [coords[0], lower_lim, coords[1]]
    print(f"Starting at <0,{lower_lim},0>", curr_block)
except ChunkDoesNotExist:
    print(f"ERROR Chunk <0,0> DOES NOT EXIST")
    exit(1)

#do 0,0
for i in range(lower_lim,upper_lim+1):
    curr_block = [coords[0], i, coords[1]]
    print(curr_block)
    
#do x-axis
coords = [-1,0]

while(True):
    try:
        if coords[1] > upper_lim or coords[1] < lower_lim or coords[0] > upper_lim or coords[0] < lower_lim:
            raise ChunkDoesNotExist
            
        for i in range(lower_lim,upper_lim+1):
            curr_block = [coords[0], i, coords[1]]
            print(curr_block)
            
        coords[0]+=dirX
    except ChunkDoesNotExist:
        if dirX == -1:
            coords = [1,0]
            dirX = 1
        else:
            break
        
        
#do y-axis
coords = [0,-1]

while(True):
    try:
        if coords[1] > upper_lim or coords[1] < lower_lim or coords[0] > upper_lim or coords[0] < lower_lim:
            raise ChunkDoesNotExist
            
        for i in range(lower_lim,upper_lim+1):
            curr_block = [coords[0], i, coords[1]]
            print(curr_block)
            
        coords[1]+=dirY
    except ChunkDoesNotExist:
        if dirY == -1:
            coords = [0,1]
            dirY = 1
        else: 
            break

#Quadrant by Quadrant
print("starting Quadrant Search")
coords = [-1,-1]
dirX = -1
dirY = -1

while(True):
    try:
        if coords[1] > upper_lim or coords[1] < lower_lim or coords[0] > upper_lim or coords[0] < lower_lim:
            raise ChunkDoesNotExist
            
        for i in range(lower_lim,upper_lim+1):
            curr_block = [coords[0], i, coords[1]]
            print(curr_block)
            
        coords[0]+=dirX
    except ChunkDoesNotExist: 
        #X gone out of range
        if dirX == -1:
            #swap directions
            coords[0] = 1
            dirX = 1
        else: 
            try:
                #reset X, increment y in certain direction
                dirX = -1
                coords[0] = 0
                coords[1]+=dirY
                
                #test if in Range
                if coords[1] > upper_lim or coords[1] < lower_lim or coords[0] > upper_lim or coords[0] < lower_lim:
                    raise ChunkDoesNotExist
                    
                coords[0] = -1
            except ChunkDoesNotExist:
                coords[0] = -1
                
                #Y gone out of range
                if dirY == -1:
                    #swap directions
                    coords[1] = 1
                    dirY = 1
                else:
                    #end program, everything should be done 
                    break
            
            
                
                
        
        
        
