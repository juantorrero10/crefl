# crefl (C Reflection Engine) v0.1.0

**crefl** is a lightweight, efficient reflection engine built for C. 

Because C does not natively support reflection, `crefl` bridges this gap by statically analyzing your code and providing a runtime mechanism to inspect types, structures, and metadata dynamically.

## 🧠 Architecture Overview

The `crefl` engine is broken down into three primary components working together to bring reflection to C:

### 1. Python Metadata Generator
Instead of relying on heavy compiler plugins, `crefl` uses a dedicated Python script to parse your C headers and source files. It extracts type information, struct layouts, and field names, acting as the frontend of the reflection pipeline.

### 2. The `.CREFL` Binary Format
Once the Python generator extracts the type data, it packs this information into a highly optimized, custom binary format with the `.CREFL` extension. This format is designed specifically for fast parsing, minimal memory overhead, and seamless integration into C projects.

### 3. C Runtime Library
The final piece is the C runtime. By loading the `.CREFL` binary at startup, the `crefl` runtime allows your C programs to query their own types, check variable types at runtime, and safely inspect memory layouts dynamically without sacrificing performance.

## Installing dependencies

Python dependencies: pycparser argparse
C dependencies: GCC, Make

## Integrating into a proyect

Add the command python3 -i [<Source files and headers>] -o [<Output file>]
to your build chain.
Loading onto C:
```c
#include <crefl/core.h>

int main()
{
    // Load the metadata file globally
    refl_load_metadata(/* .crefl file path */);
    // Set up the runtime for the current thread
    refl_threadlocal_init(0);

    // ...
}

```