#ifndef TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_
#define TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_

#include <algorithm>
#include <span>

/**
 * Compresses given data at compile-time, while also providing utilities for decoding.
 * @tparam data Expected to be a null-terminated `char` of data to be compressed.
 */
template<auto data>
class huffman_compress
{
    using size_t = unsigned long int;

    // The internals for this class needed to be defined before they're used in
    // the public interface. Scroll to the next `public` section for usable variables/functions.
private:
    // Node structure used for tree-building.
    struct node {
        int value = 0;
        size_t freq = 0;

        int parent = -1;
        int left = -1;
        int right = -1;
    };

    // Builds a list of nodes for every character that appears in the data.
    // This list is sorted by increasing frequency.
    consteval static auto build_node_list() {
        auto table = std::span(new node[256] {}, 256);
        for (int i = 0; i < 256; i++)
            table[i].value = i;
        for (size_t i = 0; data[i]; i++)
            table[data[i]].freq++;
        std::sort(table.begin(), table.end(), [](auto& a, auto& b) { return a.freq < b.freq; });
        int empty_count;
        for (empty_count = 0; table[empty_count].freq == 0; empty_count++);
        auto iter = std::copy(table.begin() + empty_count, table.end(), table.begin());
        std::fill(iter, table.end(), node());
        return table;
    }
    // Returns the count of how many nodes in build_node_list() are valid nodes.
    consteval static auto node_count() {
        auto table = build_node_list();
        size_t i;
        for (i = 0; table[i].value != 0; i++);
        delete[] table.data();
        return i;
    }
    // Builds a tree out of the node list, allowing for compression and decompression.
    consteval static auto build_node_tree() {
        auto table = build_node_list();
        
        auto end = node_count();
        size_t endend = 255;
        unsigned char endv = 0xFF;
        while (table[1].freq != 0) {
            node n { endv--,
                     table[0].freq + table[1].freq,
                     -1,
                     table[0].value,
                     table[1].value };
            table[endend--] = table[0];
            table[endend--] = table[1];
            size_t insert;
            for (insert = 0;
                 table[insert].freq != 0 && table[insert].freq < n.freq;
                 insert++);
            std::copy_backward(table.begin() + insert,
                               table.begin() + end,
                               table.begin() + end + 1);
            table[insert] = n;
            std::copy(table.begin() + 2, table.begin() + end + 1, table.begin());
            table[end - 1] = node();
            table[end--] = node();
        }
        std::copy(table.begin() + endend + 1, table.end(), table.begin() + 1);

        for (size_t i = 1; i < 256 - endend; i++) {
            if (table[i].parent == -1) {
                for (size_t j = 0; j < i; j++) {
                    if (table[j].left == table[i].value || table[j].right == table[i].value) {
                        table[i].parent = j;
                        break;
                    }
                }
            }
        }

        return table;
    }
    // Returns the count of how many nodes are in the node tree.
    consteval static auto tree_count() {
        auto table = build_node_tree();
        size_t i;
        for (i = 0; i < 256 && table[i].value != 0; i++);
        delete[] table.data();
        return i;
    }
    // Determines the size of the compressed data.
    // Returns a pair: [total byte size, bits used in last byte].
    consteval static auto output_size() {
        auto tree = build_node_tree();
        size_t bytes = 0, bits = 0;
        for (size_t i = 0; i < std::char_traits<char>::length(data); i++) {
            auto leaf = std::find_if(tree.begin(), tree.end(), [c = data[i]](auto& n) { return n.value == c; });
            while (leaf->parent != -1) {
                if (++bits == 8)
                    bits = 0, bytes++;
                leaf = tree.begin() + leaf->parent;
            }
        }

        delete[] tree.data();
        return std::make_pair(bytes + 1, bits);
    }
    // Compresses the input data, placing the result in `output`.
    consteval void compress()
    {
        auto tree = build_node_tree();
        size_t bytes = size();
        int bits = 5;
        for (size_t i = std::char_traits<char>::length(data); i > 0; i--) {
            auto leaf = std::find_if(tree.begin(), tree.begin() + tree_count(), [c = data[i - 1]](auto& n) { return n.value == c; });
            while (leaf->parent != -1) {
                auto parent = tree.begin() + leaf->parent;
                if (parent->right == leaf->value)
                    output[bytes - 1] |= (1 << bits);
                if (++bits == 8) {
                    bits = 0;
                    if (--bytes == 0)
                        return;
                }
                leaf = parent;
            }
        }
        delete[] tree.data();
    }
    // Builds the tree that can be used for decompression, stored in `decode_tree`.
    consteval void build_decode_tree() {
        auto tree = build_node_tree();
        for (size_t i = 0; i < tree_count(); i++) {
            decode_tree[i] = tree[i].value;
            decode_tree[i + 1] = std::max(tree[i].left, 0);
            decode_tree[i + 1] = std::max(tree[i].right, 0);
        }
        delete[] tree.data();
    }

public:
    // Returns the size of the compressed data, in bytes.
    consteval static auto size() { return output_size().first; }
    // Returns how many of the bits in the last byte of `output` are actually part of the data.
    consteval static auto lastbitscount() { return output_size().second; }

    // Contains the compressed data.
    unsigned char output[size()] = {};
    // Contains a 'tree' that can be used to decompress the data.
    unsigned char decode_tree[3 * tree_count()] = {};

    consteval huffman_compress() {
        build_decode_tree();
        compress();
    }
};

#endif // TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_
