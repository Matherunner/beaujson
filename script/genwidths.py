import argparse

parser = argparse.ArgumentParser()
parser.add_argument("path", help="Path to EastAsianWidth.txt")
args = parser.parse_args()

with open(args.path, "r") as file:
    for line in file:
        comment_idx = line.find("#")
        if comment_idx != -1:
            line = line[:comment_idx]
        line = line.strip()
        if not line:
            continue
        fields = [x.strip() for x in line.split(";")]
        range = fields[0].split("..")
        kind = fields[1]
        if kind != "W" and kind != "F":
            continue
        print(range, kind)
