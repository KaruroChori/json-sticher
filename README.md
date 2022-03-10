# json-stitcher
This tool joins JSON fragments into a single file.

## Features

- Support JSON and BSON fragments.
- Can integrate binary data as fragments via base64 encoding.
- Optional evaluation of schemas for each fragment.
- Simple integration of metadata and documentation for each fragment.
- Support for templates, improving the re-usability of fragments.
- Specialization mechanisms for template instances, allowing copies to alter some of the original content.
- Implicit construction. No directive in the json file needed. It can be applied to virtually any json file.

## Compiling (Linux)

```
mkdir build
cd build
cmake ..
make -j32
```
## Help & Usage

```
json-stitcher -h
```

### Example
```
./json-stitcher -f ../examples/test-1 -v
```

### Templates
Templates are defined via symbolic links. Please refer to `./examples/test-template` to see it in action.

## Flags
TBW
