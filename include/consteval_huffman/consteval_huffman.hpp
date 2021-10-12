/**
 * consteval_huffman.hpp - Provides compile-time text compression.
 * Written by Clyne Sullivan.
 * https://github.com/tcsullivan/consteval-huffman
 */

#ifndef TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_
#define TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_

#include <algorithm>
#include <concepts>
#include <span>
#include <type_traits>

namespace detail
{
    // Provides a string container for the huffman compressor.
    // Using this allows for automatic string data length measurement, as
    // well as implementation of the _huffman suffix.
    template<typename T, unsigned long int N>
        requires(std::same_as<std::remove_cvref_t<T>, char> ||
                 std::same_as<std::remove_cvref_t<T>, unsigned char>)
    struct huffman_string_container {
        T data[N];
        consteval huffman_string_container(const T (&s)[N]) noexcept {
            std::copy(s, s + N, data);
        }
        consteval operator const T *() const noexcept {
            return data;
        }
        consteval auto size() const noexcept {
            return N;
        }
    };
}

/**
 * Compresses the given data string using Huffman coding, providing a
 * minimal run-time interface for decompressing the data.
 * @tparam raw_data The string of data to be compressed.
 */
template<auto raw_data>
    requires(
        std::same_as<std::remove_cvref_t<decltype(raw_data)>,
            detail::huffman_string_container<std::remove_cvref_t<decltype(raw_data.data[0])>,
                raw_data.size()>> &&
        raw_data.size() > 0)
class huffman_compressor
{
    using size_t = long int;
    using usize_t = unsigned long int;

    // Note: class internals need to be defined before the public interface.
    // See the bottom of the class definition for usage.
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
    consteval static auto build_node_list() noexcept {
        // Build a list for counting every occuring value
        auto list = std::span(new node[256] {}, 256);
        for (int i = 0; i < 256; i++)
            list[i].value = i;
        for (usize_t i = 0; i < raw_data.size(); i++)
            list[raw_data[i]].freq++;

        std::sort(list.begin(), list.end(),
            [](const auto& a, const auto& b) { return a.freq < b.freq; });

        // Filter out the non-occuring values, and build a compact list to return
        auto first_valid_node = std::find_if(list.begin(), list.end(),
            [](const auto& n) { return n.freq != 0; });
        auto fit_size = std::distance(first_valid_node, list.end());
        if (fit_size < 2)
            fit_size = 2;
        auto fit_list = std::span(new node[fit_size] {}, fit_size);
        std::copy(first_valid_node, list.end(), fit_list.begin());
        delete[] list.data();
        return fit_list;
    }

    /**
     * Returns the count of how many nodes are in the node tree.
     */
    consteval static auto tree_count() noexcept {
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
    consteval static auto build_node_tree() noexcept {
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
                    [&iter](const auto& n) {
                        return n.left == iter->value || n.right == iter->value;
                    });
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
    consteval static auto compressed_size_info() noexcept {
        auto tree = build_node_tree();
        size_t bytes = 1, bits = 0;

        for (usize_t i = 0; i < raw_data.size(); i++) {
            auto c = static_cast<int>(raw_data[i]);
            auto leaf = std::find_if(tree.begin(), tree.end(),
                [c](const auto& n) { return n.value == c; });

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
    consteval void compress() noexcept {
        auto tree = build_node_tree();

        // Set up byte and bit count (note, we're compressing the data backwards)
        auto [bytes, bits] = compressed_size_info();
        if (bits > 0)
            bits = 8 - bits;
        else
            bits = 0, bytes--;

        // Compress data backwards, because we obtain the Huffman codes backwards
        // as we traverse towards the parent node.
        for (auto i = raw_data.size(); i > 0; i--) {
            auto c = static_cast<int>(raw_data[i - 1]);
            auto leaf = std::find_if(tree.begin(), tree.end(),
                [c](auto& n) { return n.value == c; });

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
    consteval void build_decode_tree() noexcept {
        auto tree = build_node_tree();
        auto decode_tree = compressed_data + compressed_size_info().first;

        for (usize_t i = 0; i < tree_count(); i++) {
            // Only store node value if it represents a data value
            decode_tree[i * 3] = tree[i].value <= 0xFF ? tree[i].value : 0;

            usize_t j;
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

public:
    consteval static auto compressed_size() noexcept {
        return compressed_size_info().first + 3 * tree_count();
    }
    consteval static auto uncompressed_size() noexcept {
        return raw_data.size();
    }
    consteval static size_t bytes_saved() noexcept {
        size_t diff = uncompressed_size() - compressed_size();
        return diff > 0 ? diff : 0;
    }

    // Utility for decoding compressed data.
    class decoder {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = int;

        decoder(const unsigned char *comp_data) noexcept
            : m_data(comp_data),
              m_table(comp_data + compressed_size_info().first) { get_next(); }
        decoder() = default;

        constexpr static decoder end(const unsigned char *comp_data) noexcept {
            decoder ender;
            ender.m_data = comp_data;
            if constexpr (bytes_saved() > 0) {
                const auto [size_bytes, last_bits] = compressed_size_info();
                ender.m_data += size_bytes - 1;
                ender.m_bit = 1 << (7 - last_bits);
            } else {
                ender.m_data += raw_data.size();
            }

            return ender;
        }

        bool operator==(const decoder& other) const noexcept {
            return m_data == other.m_data &&
                m_bit == other.m_bit &&
                m_current == other.m_current;
        }
        auto operator*() const noexcept {
            return m_current;
        }
        decoder& operator++() noexcept {
            get_next();
            return *this;
        }
        decoder operator++(int) noexcept {
            auto old = *this;
            get_next();
            return old;
        }

    private:
        void get_next() noexcept {
            if (auto e = end(m_table - compressed_size_info().first);
                m_data == e.m_data && m_bit == e.m_bit)
            {
                m_current = -1;
                return;
            }
            if constexpr (bytes_saved() > 0) {
                auto *node = m_table;
                int data = *m_data;
                auto bit = m_bit;
                do {
                    node += (data & bit) ? node[2] * 3u : node[1] * 3u;
                    bit >>= 1;
                    if (!bit)
                        bit = 0x80, data = *++m_data;
                } while (node[1] != 0);
                m_bit = bit;
                m_current = *node;
            } else {
                m_current = *m_data++;
            }
        }

        const unsigned char *m_data = nullptr;
        const unsigned char *m_table = nullptr;
        unsigned char m_bit = 0x80;
        int m_current = -1;

        friend class huffman_compressor;
    };

    // Stick the forward_iterator check here just so it's run
    consteval huffman_compressor() noexcept
        requires (std::forward_iterator<decoder>)
    {
        if constexpr (bytes_saved() > 0) {
            build_decode_tree();
            compress();
        } else {
            std::copy(raw_data.data, raw_data.data + raw_data.size(),
                compressed_data);
        }
    }

    auto begin() const noexcept {
        return decoder(compressed_data);
    }
    auto end() const noexcept {
        return decoder::end(compressed_data);
    }
    auto cbegin() const noexcept { return begin(); }
    auto cend() const noexcept { return end(); }

    // For accessing the compressed data
    auto data() const noexcept {
        if constexpr (bytes_saved() > 0)
            return compressed_data;
        else
            return raw_data;
    }

    auto size() const noexcept {
        if constexpr (bytes_saved() > 0)
            return compressed_size();
        else
            return uncompressed_size();
    }

private:
    // Contains the compressed data, followed by the decoding tree.
    unsigned char compressed_data[
        bytes_saved() > 0 ? compressed_size_info().first + 3 * tree_count()
                          : raw_data.size()] = {0};
};

template <detail::huffman_string_container hsc>
constexpr auto operator ""_huffman()
{
    return huffman_compressor<hsc>();
}

template <detail::huffman_string_container hsc>
constexpr auto huffman_compress = huffman_compressor<hsc>();

namespace detail
{
    template <typename T, T... list>
    class huffman_compress_array_container {
    private:
        constexpr static T uncompressed[] = {list...};
    public:
        constexpr static auto data = huffman_compress<uncompressed>;
    };
}
template <typename T, T... list>
constexpr auto huffman_compress_array = detail::huffman_compress_array_container<T, list...>::data;

#endif // TCSULLIVAN_CONSTEVAL_HUFFMAN_HPP_
