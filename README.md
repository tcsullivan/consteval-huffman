# consteval-huffman

A C++20 utility for compressing string literals at compile-time to save program space. The compressed data can be decompressed at run-time through the use of a decoder that follows `std::forward_iterator`.

Compression is achieved using [Huffman coding](https://en.wikipedia.org/wiki/Huffman_coding), which works by creating short codes (measured in bits) for frequently-occuring characters. This works best on larger pieces of data that are limited in their range of values (e.g. written text).

## Requirements

A C++20 compliant compiler. `consteval-huffman` is confirmed to work on gcc 10.1 and later, doesn't work yet on clang.

## Use cases

**1. Text configurations**

A ~3.5kB string of JSON can be compressed down ~2.5kB ([see it on Godbolt](https://godbolt.org/z/rqWf4v)).

**2. Scripts**

A ~40 line comment-including sample of Lisp can be reduced from 1,662 bytes to 1,251 (412 bytes saved) ([Godbolt](https://godbolt.org/z/Kbenbh)).

**3. Numbers?**

A string of numbers 1 to 100 separated with spaces can be compressed to 64% of its original size ([Godbolt](https://godbolt.org/z/Te17aM)).

## How to Use

```cpp
// 1. Include
#include <consteval_huffman/consteval_huffman.hpp>

// 2. Use _huffman suffix (data now stores compressed string)
auto data = "This is my string of data"_huffman; // "\0\x1 Non-text data works too!"

// 3. Use iterator to decompress at run-time
for (char c : data)
    std::cout << c;
```

Use `data.begin()` or `data.cbegin()` to get an iterator for the data which decompresses the next byte with every increment.  
These of course come with `end()` and `cend()`.

Use `data()` to get a pointer to the *compressed* data.

Use `size()` to get the size of the compressed data.

Should compression not decrease the size of the given data, the data will be stored uncompressed. The above functions will still behave as they should.
