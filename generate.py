import os
from enum import Enum

with open("src/sokol_image.h", "r") as fh:
    lines = [line.rstrip() for line in fh.readlines()]

first_half = []
second_half = []

class ReadState(Enum):
    FIRST_HALF = 0,
    SKIPPING = 1,
    SECOND_HALF = 2

state = ReadState.FIRST_HALF
for line in lines:
    match state:
        case ReadState.FIRST_HALF:
            if line == "// INCLUDES":
                state = ReadState.SKIPPING
            else:
                first_half.append(line)
        case ReadState.SKIPPING:
            if not line or line.isspace():
                state = ReadState.SECOND_HALF
        case ReadState.SECOND_HALF:
            second_half.append(line)

deps = {"stb_image.h": "STB_IMAGE_IMPLEMENTATION",
        "stb_image_write.h": "STB_IMAGE_WRITE_IMPLEMENTATION",
        "qoi.h": "QOI_IMPLEMENTATION"}

with open("sokol_image.h", "w") as fh:
    fh.write("\n".join(first_half))
    fh.write("\n")
    for dep, define in deps.items():
        fh.write(f"#define {define}\n")
        with open(f"src/{dep}", "r") as sfh:
            fh.write(sfh.read())
        fh.write("\n")
        fh.write("\n".join(second_half))

