import sys
import mcschematic

def generate(input_filename, output_filename):
    line_split  = open(input_filename, "r").read().splitlines()
    schem = mcschematic.MCSchematic()
    current_line_in_layer = 0
    current_layer = 0

    for line in line_split:
        current_torch_in_line = 0
        for c in list(line.replace(" ", "")):
            if current_torch_in_line > 23:
                raise Exception("Expected to end at 24th bit of line")
            
            if c == '1':
                coordinates = (
                    current_line_in_layer * 2 + 1,       # instruction index in layer 
                    -1 * (current_layer * 5 + 1),        # current layer
                    -1 * (current_torch_in_line * 2 + 1) # the bit that is being processed
                )
                print(f"Adding redstone torch to {coordinates[0]}, ~{coordinates[1]}, ~{coordinates[2]}")
                schem.setBlock(coordinates, "minecraft:redstone_wall_torch[facing=east]")
            current_torch_in_line += 1
        
        # move to the next instruction
        current_line_in_layer += 1
        if current_line_in_layer > 255:
            current_line_in_layer = 0
            current_layer += 1
            if current_layer > 3:
                raise Exception("Exceeded number of instructions encodable")
    
    schem.save(".", output_filename, mcschematic.Version.JE_1_17)

if len(sys.argv) < 3:
    print("Expected atleast two arguments")
else:
    generate(sys.argv[1], sys.argv[2])
