# consteval-huffman

Allows for long string or data constants to be compressed at compile-time, with a small decoder routine for decompression at run-time. 

Compression is achieved using [Huffman coding](https://en.wikipedia.org/wiki/Huffman_coding), which works by creating short (measured in bits) codes for frequently-occuring characters.

## Use cases

**1. Text configurations (e.g. JSON)**

A ~3.5kB string of JSON can be compressed down ~2.5kB ([see it on Godbolt](https://godbolt.org/z/P6a9Kr)).

**2. Scripts (e.g. Lisp)**

A ~40 line commented sample of Lisp can be reduced from 1,662 bytes to 1,244 (418 bytes saved) ([on Godbolt](https://godbolt.org/z/c64Pzz)).

Compression will work best on larger blocks of text or data, as a decoding tree must be stored with the compressed data that requires three bytes per unique data byte.

## How to Use

```cpp
#include "consteval_huffman.hpp"

constexpr static const char some_data_raw[] = /* insert text here */;

constinit static const auto some_data = consteval_huffman<some_data_raw>();

// Or, with a set data length:
// ... some_data = consteval_huffman<some_data_raw, 1500>();

int main()
{
    // Decompress and print out the data
    for (auto decode = some_data.get_decoder(); decode; ++decode)
        putchar(*decode);
}
```
