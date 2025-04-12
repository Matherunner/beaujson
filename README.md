# beaujson

## Building

Dependencies are added as subtrees under vendor, so no action needed to fetch them.

To build for release,

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```
