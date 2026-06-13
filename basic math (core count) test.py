import math

length = 10
cpu_count = 2

segcount = math.ceil(length/cpu_count)

segs = [(seg*segcount, min(seg * segcount + segcount - 1, length - 1)) for seg in range (0, cpu_count)]

print(segs)