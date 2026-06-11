import os
from pathlib import Path

filename = Path(input("Input file path: ").strip())

input_path = Path(input("Input file path: ").strip()).expanduser().resolve()
filename = input_path.name

levelMin = 2**31 - 1

levelMax = -2**31

with open(f"{input_path}", "r") as file:
    for x in file: 
        level = int(x.split(".")[1])

        if levelMax < level:
            levelMax = level
        
        if levelMin > level:
            levelMin = level

path = Path("segs/")
path.mkdir(parents=True, exist_ok=True)


segments = [path / f"seg[{i}]_{filename}" for i in range(levelMin, levelMax+1)]

with open(f"{input_path}", "r") as file:
    f_segs = [open(segment, "w") for segment in segments]
    for x in file:
        f_segs[int(x.split(".")[1]) - levelMin].write(x)

    for seg in f_segs:
        seg.close()

with open(f"sorted_{filename}", "w" ) as file:
    for segment in reversed(segments):
            seg = open(segment, "r") 
            for x in seg:
                file.write(x)

            seg.close()
            os.remove(segment)


input("COMPLETE! Press <enter> to close...")
