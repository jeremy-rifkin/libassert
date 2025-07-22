#ifndef PATHS_HPP
#define PATHS_HPP

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.hpp"

#include <libassert/assert.hpp>

LIBASSERT_BEGIN_NAMESPACE
namespace detail {
    using path_components = std::vector<std::string>;

    path_components parse_path(const std::string_view path);

    class path_trie {
        // Backwards path trie structure
        // e.g.:
        // a/b/c/d/e     disambiguate to -> c/d/e
        // a/b/f/d/e     disambiguate to -> f/d/e
        //  2   2   1   1   1
        // e - d - c - b - a
        //      \   1   1   1
        //       \ f - b - a
        // Nodes are marked with the number of downstream branches
        size_t downstream_branches = 1;
        std::string root;
        std::unordered_map<std::string, std::unique_ptr<path_trie>> edges;
    public:
        explicit path_trie(std::string _root) : root(std::move(_root)) {};
        void insert(const path_components& path);
        path_components disambiguate(const path_components& path);
    private:
        void insert(const path_components& path, int i);
    };

    class identity_path_handler : public path_handler {
    public:
        std::unique_ptr<detail::path_handler> clone() const override;
        std::string_view resolve_path(std::string_view) override;
    };

    class disambiguating_path_handler : public path_handler {
        std::vector<std::string> paths;
        std::unordered_map<std::string, std::string> path_map;
    public:
        std::unique_ptr<detail::path_handler> clone() const override;
        std::string_view resolve_path(std::string_view) override;
        bool has_add_path() const override;
        void add_path(std::string_view) override;
        void finalize() override;
    };

    class basename_path_handler : public path_handler {
    public:
        std::unique_ptr<detail::path_handler> clone() const override;
        std::string_view resolve_path(std::string_view) override;
    };
}
LIBASSERT_END_NAMESPACE

#endif
