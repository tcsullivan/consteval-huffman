# consteval-huffman

Allows for long strings of text or data to be compressed at compile-time, with an efficient decoder routine for decompression at run-time. The decoder conforms to `std::forward_iterator`, allowing for use in ranged-for or standard algorithms.

Compression is achieved using [Huffman coding](https://en.wikipedia.org/wiki/Huffman_coding), which works by creating short codes (measured in bits) for frequently-occuring characters. This works best on larger pieces of data, or more so data that is limited in its range of values (e.g. written text).

## Use cases

**1. Text configurations (e.g. JSON)**

A ~3.5kB string of JSON can be compressed down ~2.5kB ([see it on Godbolt](https://godbolt.org/z/rqWf4v)).

**2. Scripts (e.g. Lisp)**

A ~40 line comment-including sample of Lisp can be reduced from 1,662 bytes to 1,244 (418 bytes saved) ([Godbolt](https://godbolt.org/z/Kbenbh)).

## How to Use

```cpp
#include "consteval_huffman.hpp"

// Using the _huffman suffix creates an object with the compressed data.
// If data is not smaller after compression, the object will keep the data uncompressed.
constinit auto some_data = "This is my string of data"_huffman;

int main()
{
    // Decompress and print out the data
    for (auto c : some_data)
        putchar(c);
}
```
