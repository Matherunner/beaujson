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

## Design

We start with a few design goals.

1. Support paging up and down with very low latency.
2. Support expanding and collapsing nested structures with very low latency.
3. Support skipping past nested structures with very low latency.
4. Support extremely deeply nested structures without performance penalties.
5. Support wrapping long string values.
6. (Nice to have) Support random access by index.
7. (Nice to have) Support inserting fields.

The view model must be one that supports removing and adding chains efficiently to satisfy (2). This precludes plain arrays, as arrays don't support efficient insertions and deletions in the middle. A doubly linked list would support (1) and (2). To support (3), the start of each nested structure should contain a pointer to the end of it. For example, suppose we have the following JSON:

```json
{
    "field1": {
        "field1.1": 123,
        "field2.1": 456
    },
    "field2": "field2"
}
```

Suppose we represent each line as a node in the doubly linked list. The node corresponding to `field1` would contain a pointer to `field1.1` as well as `field2`.

To support (5), we will need to dynamically and lazily insert wrapped lines as we scroll through the doubly linked list depending on the terminal width.

To support (6), we will need a separate data structure to augment the main doubly linked list. Drawing inspiration from skip lists, suppose we have another skipping doubly linked list that contains a node every N nodes. Each edge would contain information about how many nodes were skipped in the view model. This would allow us to skip forward much faster. When a nested JSON is collapsed, we would update the width of the effected edges which is a local operation and therefore independent of the length of the entire list.

If we don't need (6), then the implementation would be considerably simpler. It'd also be easier to support lazy parsing of JSON.
