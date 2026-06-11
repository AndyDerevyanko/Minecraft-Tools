
import time
import os

filename = input("Input file path: ")

filename = os.path.basename(filename)

output = []


with open(f"{filename}", "r") as file:
    while True:
        x = file.readline()  

        if len(x) == 0:
            break

        output.append(x)


with open(f"r{filename}", "w") as out:
        for x in reversed(output):
            out.write(x)

input("COMPLETE! Press <enter> to close...")

exit(0)
