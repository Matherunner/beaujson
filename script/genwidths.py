import argparse

OUTPUT_WIDTH = 100

parser = argparse.ArgumentParser()
parser.add_argument("path", help="Path to EastAsianWidth.txt")
args = parser.parse_args()

wide_ranges = []

with open(args.path, "r") as file:
    for line in file:
        comment_idx = line.find("#")
        if comment_idx != -1:
            line = line[:comment_idx]
        line = line.strip()
        if not line:
            continue
        fields = [x.strip() for x in line.split(";")]
        r = [int("0x" + x, 16) for x in fields[0].split("..")]
        if len(r) == 1:
            r.append(r[0])
        kind = fields[1]
        if kind != "W" and kind != "F":
            continue
        wide_ranges.append(r)

wide_ranges.sort(key=lambda t: t[0])

bits = []
for r in wide_ranges:
    while len(bits) <= r[1]:
        bit = "1" if r[0] <= len(bits) <= r[1] else "0"
        bits.append(bit)

# Verification

for r in wide_ranges:
    for i in range(r[0], r[1] + 1):
        if bits[i] != "1":
            raise
for i in range(wide_ranges[0][0]):
    if bits[i] != "0":
        raise
for i in range(len(wide_ranges) - 1):
    r1 = wide_ranges[i]
    r2 = wide_ranges[i + 1]
    for j in range(r1[1] + 1, r2[0]):
        if bits[j] != "0":
            raise

output_lines = []
bits.reverse()  # Bit set constructor expects big endian
for i in range(0, len(bits), OUTPUT_WIDTH):
    output_lines.append('        "' + "".join(bits[i : i + OUTPUT_WIDTH]) + '"')
lines = "\n".join(output_lines)

output = f"""#pragma once

#include <bitset>

namespace east_asian_width
{{
    constexpr std::bitset<{len(bits)}> TABLE(
{lines}      
    );
}}
"""

print(output)
