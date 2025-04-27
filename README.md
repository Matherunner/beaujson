# beaujson

![Blah](https://github.com/Matherunner/beaujson/actions/workflows/cmake-multi-platform.yml/badge.svg)

⚠️ Alpha quality ⚠️

![image](https://github.com/user-attachments/assets/2f29334e-53ec-4c4f-8634-a4ebad88e5e4)

## Building

Dependencies are added as subtrees under vendor, so no action needed to fetch them.

To build for release,

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

## Guide

beaujson can load from

1. File, i.e. `beaujson file.json`
2. Clipboard, i.e. `beaujson`
3. stdin, i.e. `beaujson < content` or `cmd | beaujson`

Key bindings:

- `j`, <kbd>ArrowDown</kbd>, <kbd>Enter</kbd>: Scroll one line down
- `k`, <kbd>ArrowUp</kbd>: Scroll one line up
- `f`, <kbd>Space</kbd>: Scroll a page down
- `b`: Scroll a page up
- `d`: Scroll half a page down
- `u`: Scroll half a page up
- `g`: Jump to top
- `G` (<kbd>Shift</kbd> + <kbd>g</kbd>): Jump to bottom
- `q`: Quit
- `c`: Copy the primitive value (string, number, boolean, null) of the highlighted line
- `-`: Collapse all
- `+` (<kbd>Shift</kbd> + <kbd>=</kbd>): Expand all