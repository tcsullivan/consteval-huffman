
/**
 * consteval_huffman.hpp - Provides compile-time text compression.
 * Written by Clyne Sullivan.
 * https://github.com/tcsullivan/consteval-huffman
 */

#ifndef TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_
#define TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_

#include <algorithm>
#include <span>

/**
 * Compresses the given character string using Huffman coding, providing a
 * minimal run-time interface for decompressing the data.
 * @tparam data The string of data to be compressed.
 * @tparam data_length The size in bytes of the data, defaults to using strlen().
 */
template<const char *data, std::size_t data_length = std::char_traits<char>::length(data)>
class huffman_compress
{
    using size_t = unsigned long int;

    // Jump to the bottom of this header for the public-facing features of this
    // class.
    // The internals needed to be defined before they were used.
private:
    // Node structure used to build a tree for calculating Huffman codes.
    struct node {
        int value = 0;
        size_t freq = 0;

        // Below values are indices into the node list
        int parent = -1;
        int left = -1;
        int right = -1;
    };

    /**
     * Builds a list of nodes for every character that appears in the given data.
     * This list is sorted by increasing frequency.
     * @return Compile-time allocated array of nodes
     */
    consteval static auto build_node_list() {
        auto table = std::span(new node[256] {}, 256);
        for (int i = 0; i < 256; i++)
            table[i].value = i;
        for (size_t i = 0; i < data_length; i++)
            table[data[i]].freq++;

        std::sort(table.begin(), table.end(), [](auto& a, auto& b) { return a.freq < b.freq; });

        auto first_valid_node = std::find_if(table.begin(), table.end(),
            [](const auto& n) { return n.freq != 0; });
        auto iter = std::copy(first_valid_node, table.end(), table.begin());
        std::fill(iter, table.end(), node());
        return table;
    }

    /**
     * Counts how many nodes in build_node_list() are valid.
     * @return Number of valid nodes, i.e. the "size" of the list.
     */
    consteval static auto node_count() {
        auto table = build_node_list();
        size_t i;
        for (i = 0; table[i].freq != 0; i++);
        delete[] table.data();
        return i;
    }

    // Returns the count of how many nodes are in the node tree.
public:
    consteval static auto tree_count() {
        return node_count() * 2 - 1;
    }

    /**
     * Builds a tree out of the node list, allowing for the calculation of
     * Huffman codes.
     * @return Compile-time allocated tree of nodes, root node at index zero.
     */
    consteval static auto build_node_tree() {
        auto list = build_node_list();
        auto tree = std::span(new node[tree_count()] {}, tree_count());
        
        auto list_end = node_count();
        auto tree_begin = tree.end();
        int next_merged_node_value = 0x100;
        while (list[1].freq != 0) {
            // Create the merged parent node
            node new_node {
                next_merged_node_value++,
                list[0].freq + list[1].freq,
                -1,
                list[0].value,
                list[1].value
            };

            *--tree_begin = list[0];
            *--tree_begin = list[1];

            auto insertion_point = list.begin();
            while (insertion_point->freq != 0 && insertion_point->freq < new_node.freq)
                insertion_point++;

            std::copy_backward(insertion_point, list.begin() + list_end, list.begin() + list_end + 1);
            *insertion_point = new_node;

            std::copy(list.begin() + 2, list.begin() + list_end + 1, list.begin());
            list[list_end - 1] = node();
            list[list_end--] = node();
        }

        // Connect child nodes to their parents
        tree[0] = list[0];
        for (auto iter = tree.begin(); ++iter != tree.end();) {
            if (iter->parent == -1) {
                auto parent = std::find_if(tree.begin(), iter,
                    [&iter](const auto& n) { return n.left == iter->value || n.right == iter->value; });
                if (parent != iter)
                    iter->parent = std::distance(tree.begin(), parent);
            }
        }

        delete[] list.data();
        return tree;
    }

    /**
     * Determines the size of the compressed data.
     * Returns a pair: [total byte size, bits used in last byte].
     */
    consteval static auto output_size() {
        auto tree = build_node_tree();
        size_t bytes = 1, bits = 0;
        for (size_t i = 0; i < data_length; i++) {
            auto leaf = std::find_if(tree.begin(), tree.end(),
                                     [c = data[i]](auto& n) { return n.value == c; });
            while (leaf->parent != -1) {
                if (++bits == 8)
                    bits = 0, bytes++;
                leaf = tree.begin() + leaf->parent;
            }
        }

        delete[] tree.data();
        return std::make_pair(bytes, bits);
    }
    // Compresses the input data, placing the result in `output`.
    consteval void compress()
    {
        auto tree = build_node_tree();
        size_t bytes = output_size().first;
        int bits;
        if (auto bitscount = output_size().second; bitscount > 0)
            bits = 8 - bitscount;
        else
            bits = 0, bytes--;
        for (size_t i = data_length; i > 0; i--) {
            auto leaf = std::find_if(tree.begin(), tree.end(),
                [c = data[i - 1]](auto& n) { return n.value == c; });
            while (leaf->parent != -1) {
                auto parent = tree.begin() + leaf->parent;
                if (parent->right == leaf->value)
                    output[bytes - 1] |= (1 << bits);
                if (++bits == 8)
                    bits = 0, --bytes;
                leaf = parent;
            }
        }
        delete[] tree.data();
    }
    // Builds the tree that can be used for decompression, stored in `decode_tree`.
    consteval void build_decode_tree() {
        auto tree = build_node_tree();

        for (size_t i = 0; i < tree_count(); i++) {
            decode_tree[i * 3] = tree[i].value <= 0xFF ? tree[i].value : 0;

            size_t j;
            for (j = i + 1; j < tree_count(); j++) {
                if (tree[i].left == tree[j].value)
                    break;
            }
            decode_tree[i * 3 + 1] = j < tree_count() ? j - i : 0;
            for (j = i + 1; j < tree_count(); j++) {
                if (tree[i].right == tree[j].value)
                    break;
            }
            decode_tree[i * 3 + 2] = j < tree_count() ? j - i : 0;
        }
        delete[] tree.data();
    }

    // Contains the compressed data.
    unsigned char output[output_size().first] = {};
    // Contains a 'tree' that can be used to decompress the data.

public:
    unsigned char decode_tree[3 * tree_count()] = {};
    // Utility for decoding compressed data.
    class decode_info {
    public:
        decode_info(const huffman_compress<data, data_length>& comp_data) :
            m_data(comp_data) { get_next(); }

        // Checks if another byte is available
        operator bool() const {
            const auto [size_bytes, last_bits_count] = m_data.output_size();
            return m_pos < (size_bytes - 1) || m_bit > (8 - last_bits_count);
        }
        // Gets the current byte
        int operator*() const { return m_current; }
        // Moves to the next byte
        int operator++() {
            get_next();
            return m_current;
        }

    private:
        // Internal: moves to next byte
        void get_next() {
            auto *node = m_data.decode_tree;
            do {
                bool bit = m_data.output[m_pos] & (1 << (m_bit - 1));
                if (--m_bit == 0)
                    m_bit = 8, m_pos++;
                node += 3 * node[bit ? 2 : 1];
            } while (node[1] != 0);
            m_current = *node;
        }

        const huffman_compress<data>& m_data;
        size_t m_pos = 0;
        unsigned char m_bit = 8;
        int m_current = -1;

        friend class huffman_compress;
    };

    consteval huffman_compress() {
        build_decode_tree();
        compress();
    }

    consteval static auto compressed_size() {
        return output_size().first + output_size().second;
    }
    consteval static auto uncompressed_size() {
        return data_length;
    }
    consteval static auto bytes_saved() {
        return uncompressed_size() - compressed_size();
    }

    // Creates a decoder object for iteratively decompressing the data.
    auto get_decoder() const {
        return decode_info(*this);
    }
};

#endif // TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_

