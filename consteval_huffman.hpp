
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
template<const char *data, auto data_length = std::char_traits<char>::length(data)>
class huffman_compress
{
    using size_t = long int;

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
        // Build a list for counting every occuring value
        auto list = std::span(new node[256] {}, 256);
        for (int i = 0; i < 256; i++)
            list[i].value = i;
        for (size_t i = 0; i < data_length; i++)
            list[data[i]].freq++;

        std::sort(list.begin(), list.end(),
            [](const auto& a, const auto& b) { return a.freq < b.freq; });

        // Filter out the non-occuring values, and build a compact list to return
        auto first_valid_node = std::find_if(list.begin(), list.end(),
            [](const auto& n) { return n.freq != 0; });
        auto fit_size = std::distance(first_valid_node, list.end());
        auto fit_list = std::span(new node[fit_size] {}, fit_size);
        std::copy(first_valid_node, list.end(), fit_list.begin());
        delete[] list.data();
        return fit_list;
    }

    /**
     * Returns the count of how many nodes are in the node tree.
     */
    consteval static auto tree_count() {
        auto list = build_node_list();
        auto count = list.size() * 2 - 1;
        delete[] list.data();
        return count;
    }

    /**
     * Builds a tree out of the node list, allowing for the calculation of
     * Huffman codes.
     * @return Compile-time allocated tree of nodes, root node at index zero.
     */
    consteval static auto build_node_tree() {
        auto list = build_node_list();
        auto tree = std::span(new node[tree_count()] {}, tree_count());
        
        auto list_end = list.end(); // Track end of list as it shrinks
        auto tree_begin = tree.end(); // Build tree from bottom
        int next_parent_node_value = 0x100; // Give parent nodes unique ids
        while (1) {
            // Create parent node for two least-occuring values
            node new_node {
                next_parent_node_value++,
                list[0].freq + list[1].freq,
                -1,
                list[0].value,
                list[1].value
            };

            // Move the two nodes into the tree and remove them from the list
            *--tree_begin = list[0];
            *--tree_begin = list[1];
            std::copy(list.begin() + 2, list_end--, list.begin());
            if (std::distance(list.begin(), list_end) == 1) {
                list.front() = new_node;
                break;
            }

            // Insert the parent node back into the list
            auto insertion_point = std::find_if(list.begin(), list_end - 1,
                [&new_node](const auto& n) { return n.freq >= new_node.freq; });
            if (insertion_point != list_end - 1) {
                *(list_end - 1) = node();
                std::copy_backward(insertion_point, list_end - 1, list_end);
            }

            *insertion_point = new_node;
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
     * @return A pair of total bytes used, and bits used in last byte.
     */
    consteval static auto compressed_size_info() {
        auto tree = build_node_tree();
        size_t bytes = 1, bits = 0;

        for (size_t i = 0; i < data_length; i++) {
            auto leaf = std::find_if(tree.begin(), tree.end(),
                [c = data[i]](const auto& n) { return n.value == c; });

            while (leaf->parent != -1) {
                if (++bits == 8)
                    bits = 0, bytes++;
                leaf = tree.begin() + leaf->parent;
            }
        }

        delete[] tree.data();
        return std::make_pair(bytes, bits);
    }

    /**
     * Compresses the input data, storing the result in the object instance.
     */
    consteval void compress()
    {
        auto tree = build_node_tree();

        // Set up byte and bit count (note, we're compressing the data backwards)
        auto [bytes, bits] = compressed_size_info();
        if (bits > 0)
            bits = 8 - bits;
        else
            bits = 0, bytes--;

        // Compress data backwards, because we obtain the Huffman codes backwards
        // as we traverse towards the parent node.
        for (auto i = data_length; i > 0; i--) {
            auto leaf = std::find_if(tree.begin(), tree.end(),
                [c = data[i - 1]](auto& n) { return n.value == c; });

            while (leaf->parent != -1) {
                auto parent = tree.begin() + leaf->parent;
                if (parent->right == leaf->value)
                    compressed_data[bytes - 1] |= (1 << bits);
                if (++bits == 8)
                    bits = 0, --bytes;
                leaf = parent;
            }
        }

        delete[] tree.data();
    }

    /**
     * Builds the decode tree, used to decompress the data.
     * Format: three bytes per node.
     *     1. Node value, 2. Distance to left child, 3. Distance to right child.
     */
    consteval void build_decode_tree() {
        auto tree = build_node_tree();

        for (size_t i = 0; i < tree_count(); i++) {
            // Only store node value if it represents a data value
            decode_tree[i * 3] = tree[i].value <= 0xFF ? tree[i].value : 0;

            size_t j;
            // Find the left child of this node
            for (j = i + 1; j < tree_count(); j++) {
                if (tree[i].left == tree[j].value)
                    break;
            }
            decode_tree[i * 3 + 1] = j < tree_count() ? j - i : 0;
            // Find the right child of this node
            for (j = i + 1; j < tree_count(); j++) {
                if (tree[i].right == tree[j].value)
                    break;
            }
            decode_tree[i * 3 + 2] = j < tree_count() ? j - i : 0;
        }

        delete[] tree.data();
    }

    // Contains the compressed data.
    unsigned char compressed_data[compressed_size_info().first] = {};
    // Contains a 'tree' that can be used to decompress the data.
    unsigned char decode_tree[3 * tree_count()] = {};

public:
    // Utility for decoding compressed data.
    class decode_info {
    public:
        decode_info(const huffman_compress<data, data_length>& comp_data) :
            m_data(comp_data) { get_next(); }

        // Checks if another byte is available
        operator bool() const {
            const auto [size_bytes, last_bits] = m_data.compressed_size_info();
            return m_pos < (size_bytes - 1) || m_bit > (8 - last_bits);
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
                bool bit = m_data.compressed_data[m_pos] & (1 << (m_bit - 1));
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
        return sizeof(compressed_data) + sizeof(decode_tree);
    }
    consteval static auto uncompressed_size() {
        return data_length;
    }
    consteval static size_t bytes_saved() {
        return uncompressed_size() - compressed_size();
    }

    // Creates a decoder object for iteratively decompressing the data.
    auto get_decoder() const {
        return decode_info(*this);
    }
};

#endif // TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_

