#include "paths.hpp"

#include "common.hpp"

namespace libassert::detail {
    using path_components = std::vector<std::string>;

    #if IS_WINDOWS
     constexpr std::string_view path_delim = "/\\";
    #else
     constexpr std::string_view path_delim = "/";
    #endif

    LIBASSERT_ATTR_COLD
    path_components parse_path(const std::string_view path) {
        // Some cases to consider
        // projects/libassert/demo.cpp               projects   libassert  demo.cpp
        // /glibc-2.27/csu/../csu/libc-start.c  /  glibc-2.27 csu      libc-start.c
        // ./demo.exe                           .  demo.exe
        // ./../demo.exe                        .. demo.exe
        // ../x.hpp                             .. x.hpp
        // /foo/./x                                foo        x
        // /foo//x                                 f          x
        path_components parts;
        for(auto part : split(path, path_delim)) {
            if(parts.empty()) {
                // TODO: Maybe it could be ok to use string_view's here, have to be careful about lifetime
                // first gets added no matter what
                parts.emplace_back(part);
            } else {
                if(part.empty()) {
                    // nop
                } else if(part == ".") {
                    // nop
                } else if(part == "..") {
                    // cases where we have unresolvable ..'s, e.g. ./../../demo.exe
                    if(parts.back() == "." || parts.back() == "..") {
                        parts.emplace_back(part);
                    } else {
                        parts.pop_back();
                    }
                } else {
                    parts.emplace_back(part);
                }
            }
        }
        LIBASSERT_PRIMITIVE_ASSERT(!parts.empty());
        LIBASSERT_PRIMITIVE_ASSERT(parts.back() != "." && parts.back() != "..");
        return parts;
    }

    LIBASSERT_ATTR_COLD
    void path_trie::insert(const path_components& path) {
        LIBASSERT_PRIMITIVE_ASSERT(path.back() == root);
        insert(path, (int)path.size() - 2);
    }

    LIBASSERT_ATTR_COLD
    path_components path_trie::disambiguate(const path_components& path) {
        path_components result;
        path_trie* current = this;
        LIBASSERT_PRIMITIVE_ASSERT(path.back() == root);
        result.push_back(current->root);
        for(size_t i = path.size() - 2; i >= 1; i--) {
            LIBASSERT_PRIMITIVE_ASSERT(current->downstream_branches >= 1);
            if(current->downstream_branches == 1) {
                break;
            }
            const std::string& component = path[i];
            LIBASSERT_PRIMITIVE_ASSERT(current->edges.count(component));
            current = current->edges.at(component).get();
            result.push_back(current->root);
        }
        std::reverse(result.begin(), result.end());
        return result;
    }

    LIBASSERT_ATTR_COLD
    void path_trie::insert(const path_components& path, int i) {
        if(i < 0) {
            return;
        }
        if(!edges.count(path[i])) {
            if(!edges.empty()) {
                downstream_branches++; // this is to deal with making leaves have count 1
            }
            edges.insert({path[i], std::make_unique<path_trie>(path[i])});
        }
        downstream_branches -= edges.at(path[i])->downstream_branches;
        edges.at(path[i])->insert(path, i - 1);
        downstream_branches += edges.at(path[i])->downstream_branches;
    }

    bool path_handler::has_add_path() const {
        return false;
    }

    void path_handler::add_path(std::string_view) {
        LIBASSERT_PRIMITIVE_ASSERT(false, "Improper path_handler::add_path");
    }

    void path_handler::finalize() {
        LIBASSERT_PRIMITIVE_ASSERT(false, "Improper path_handler::finalize");
    }

    LIBASSERT_ATTR_COLD
    std::string_view identity_path_handler::resolve_path(std::string_view path) {
        return path;
    }

    LIBASSERT_ATTR_COLD
    std::string_view disambiguating_path_handler::resolve_path(std::string_view path) {
        return path_map.at(std::string(path));
    }

    bool disambiguating_path_handler::has_add_path() const {
        return true;
    }

    LIBASSERT_ATTR_COLD
    void disambiguating_path_handler::add_path(std::string_view path) {
        paths.emplace_back(path);
    }

    LIBASSERT_ATTR_COLD
    void disambiguating_path_handler::finalize() {
        // raw full path -> components
        std::unordered_map<std::string, path_components> parsed_paths;
        // base file name -> path trie
        std::unordered_map<std::string, path_trie> tries;
        for(const auto& path : paths) {
            if(!parsed_paths.count(path)) {
                auto parsed_path = parse_path(path);
                auto& file_name = parsed_path.back();
                parsed_paths.insert({path, parsed_path});
                if(tries.count(file_name) == 0) {
                    tries.insert({file_name, path_trie(file_name)});
                }
                tries.at(file_name).insert(parsed_path);
            }
        }
        // raw full path -> minified path
        std::unordered_map<std::string, std::string> files;
        for(auto& [raw, parsed_path] : parsed_paths) {
            const std::string new_path = join(tries.at(parsed_path.back()).disambiguate(parsed_path), "/");
            internal_verify(files.insert({raw, new_path}).second);
        }
        path_map = std::move(files);
        // return {files, std::min(longest_file_width, size_t(50))};
    }

    LIBASSERT_ATTR_COLD
    std::string_view basename_path_handler::resolve_path(std::string_view path) {
        auto last = path.find_last_of(path_delim);
        if(last == path.npos) {
            last = 0;
        } else {
            last++; // go after the delim
        }
        return path.substr(last);
    }
}
