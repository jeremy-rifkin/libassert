#ifndef LIBASSERT_STRINGIFICATION_HPP
#define LIBASSERT_STRINGIFICATION_HPP

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>

#include <libassert/platform.hpp>
#include <libassert/utilities.hpp>

#if defined(LIBASSERT_USE_ENCHANTUM) && defined(LIBASSERT_USE_MAGIC_ENUM)
 #error cannot use both enchantum and magic_enum at the same time
#endif

#ifdef LIBASSERT_USE_ENCHANTUM
 #include <enchantum/enchantum.hpp>
#endif

#ifdef LIBASSERT_USE_MAGIC_ENUM
 // relative include so that multiple library versions don't clash
 // e.g. if both libA and libB have different versions of libassert as a public
 // dependency, then any library that consumes both will have both sets of include
 // paths. this isn't an issue for #include <assert.hpp> but becomes an issue
 // for includes within the library (libA might include from libB)
 #if defined(__has_include) && __has_include(<magic_enum/magic_enum.hpp>)
  #include <magic_enum/magic_enum.hpp>
 #else
  #include <magic_enum.hpp>
 #endif
#endif

#ifdef LIBASSERT_USE_FMT
 #include <fmt/format.h>
#endif

#if LIBASSERT_STD_VER >= 20
 #include <compare>
#endif

#ifdef LIBASSERT_USE_STD_FORMAT
 #include <format>
#endif

// =====================================================================================================================
// || Stringification micro-library                                                                                   ||
// || Note: There is some stateful stuff behind the scenes related to literal format configuration                    ||
// =====================================================================================================================

LIBASSERT_BEGIN_NAMESPACE
    // customization point
    template<typename T> struct stringifier /*{
        std::convertible_to<std::string> stringify(const T&);
    }*/;
LIBASSERT_END_NAMESPACE

LIBASSERT_BEGIN_NAMESPACE
namespace detail {
    // What can be stringified
    // Base types:
    //  - anything string-like
    //  - arithmetic types
    //  - pointers
    //  - smart pointers
    //  - nullptr_t
    //  - std::error_code/std::error_condition
    //  - orderings
    //  - enum values
    //  User-provided stringifications:
    //   - std::ostream<< overloads
    //   - std format
    //   - fmt
    //   - libassert::stringifier
    // Containers:
    //  - std::optional
    //  - std::variant TODO
    //  - std::expected TODO
    //  - tuples and tuple-likes
    //  - anything container-like (std::vector, std::array, std::unordered_map, C arrays, .....)
    // Priorities:
    //  - libassert::stringifier
    //  - default formatters
    //  - fmt
    //  - std fmt
    //  - osteam format
    //  - instance of catch all
    // Other logistics:
    //  - Containers are only stringified if their value_type is stringifiable
    // TODO Weak pointers?

    template<typename T>
    [[nodiscard]]
    std::string do_stringify(const T& v);

    namespace stringification {
        //
        // General traits
        //
        template<typename T, bool = (std::tuple_size<T>::value > 0)>
        struct canonicalized_get_0 {
            using type = decltype(std::get<0>(std::declval<T>()));
        };
        template<typename T>
        struct canonicalized_get_0<T, false> {
            using type = void;
        };

        template<typename T, typename = void>
        inline constexpr bool is_tuple_like = false;
        template<typename T>
        inline constexpr bool is_tuple_like<
            T,
            std::void_t<
                typename std::tuple_size<T>::type, // TODO: decltype(std::tuple_size_v<T>) ?
                typename canonicalized_get_0<T>::type
            >
        > = true;

        namespace adl {
            using std::begin, std::end; // ADL
            template<typename T, typename = void>
            inline constexpr bool is_container = false;
            template<typename T>
            inline constexpr bool is_container<
                T,
                std::void_t<
                    decltype(begin(decllval<T>())),
                    decltype(end(decllval<T>()))
                >
            > = true;

            template<typename T, typename = void>
            inline constexpr bool is_begin_deref = false;
            template<typename T>
            inline constexpr bool is_begin_deref<
                T,
                std::void_t<
                    decltype(*begin(decllval<T>()))
                >
            > = true;
        }

        template<typename T, typename = void>
        inline constexpr bool can_dereference = false;
        template<typename T>
        inline constexpr bool can_dereference<
            T,
            std::void_t<
                decltype(*decllval<T>())
            >
        > = true;

        template<typename T, typename = void>
        inline constexpr bool has_ostream_overload = false;
        template<typename T>
        inline constexpr bool has_ostream_overload<
            T,
            std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>
        > = true;

        template<typename T, typename = void> inline constexpr bool has_stringifier = false;
        template<typename T>
        inline constexpr bool has_stringifier<
            T,
            std::void_t<decltype(stringifier<strip<T>>{}.stringify(std::declval<T>()))>
        > = true;

        // Following a pattern used in fmt: This is a heuristic to detect types that look like std::filesystem::path
        // This is used so that libassert doesn't have to #include <filesystem>
        template<typename T, typename = void>
        inline constexpr bool is_std_filesystem_path_like = false;
        template<typename T>
        inline constexpr bool is_std_filesystem_path_like<
            T,
            std::void_t<
                decltype(std::declval<T>().parent_path()),
                decltype(std::declval<T>().is_absolute()),
                decltype(std::declval<T>().filename())
            >
        > = std::is_convertible_v<decltype(std::declval<T>().string()), std::string_view>;

        //
        // Catch all
        //

        [[nodiscard]] LIBASSERT_EXPORT std::string stringify_unknown(std::string_view type_name);

        template<typename T>
        [[nodiscard]] std::string stringify_unknown() {
            return stringify_unknown(type_name<T>());
        }

        //
        // Basic types
        //
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::string_view);
        // without nullptr_t overload msvc (without /permissive-) will call stringify(bool) and mingw
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::nullptr_t);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(char);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(bool);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(short);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(int);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(long);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(long long);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(unsigned short);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(unsigned int);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(unsigned long);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(unsigned long long);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(float);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(double);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(long double);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::byte);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::error_code ec);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::error_condition ec);
        #if __cplusplus >= 202002L
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::strong_ordering);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::weak_ordering);
        [[nodiscard]] LIBASSERT_EXPORT std::string stringify(std::partial_ordering);
        #endif
        [[nodiscard]] LIBASSERT_EXPORT
        std::string stringify_pointer_value(const void*);

        template<typename T>
        [[nodiscard]]
        std::string stringify_smart_ptr(const T& t) {
            if(t) {
                return do_stringify(*t);
            } else {
                return "nullptr";
            }
        }

        [[nodiscard]] LIBASSERT_EXPORT
        std::string stringify_by_ostream(const void*, void(*)(std::ostream&, const void*));

        template<typename T>
        [[nodiscard]]
        std::string stringify_by_ostream(const T& t) {
            return stringify_by_ostream(
                &t,
                [] (std::ostream& os, const void* ptr) { os << *reinterpret_cast<const T*>(ptr); }
            );
        }

        [[nodiscard]] LIBASSERT_EXPORT
        std::string stringify_enum(std::string_view type_name, std::string_view underlying_value);

        template<typename E>
        [[nodiscard]] std::string stringify_enum(E e) {
            #if defined(LIBASSERT_USE_ENCHANTUM)
             std::string_view name = enchantum::to_string(e);
             if(!name.empty()) {
                 return std::string(name);
             }
            #elif defined(LIBASSERT_USE_MAGIC_ENUM)
             std::string_view name = magic_enum::enum_name(e);
             if(!name.empty()) {
                 return std::string(name);
             }
            #endif
            return stringify_enum(
                type_name<E>(),
                stringify(static_cast<std::underlying_type_t<E>>(e))
            );
        }

        //
        // Compositions of other types
        //
        // #ifdef __cpp_lib_expected
        // template<typename E>
        // [[nodiscard]] std::string stringify(const std::unexpected<E>& x) {
        //     return "unexpected " + stringify(x.error());
        // }

        // template<typename T, typename E>
        // [[nodiscard]] std::string stringify(const std::expected<T, E>& x) {
        //     if(x.has_value()) {
        //         if constexpr(std::is_void_v<T>) {
        //             return "expected void";
        //         } else {
        //             return "expected " + stringify(*x);
        //         }
        //     } else {
        //         return "unexpected " + stringify(x.error());
        //     }
        // }
        // #endif

        template<typename T>
        [[nodiscard]]
        std::string stringify(const std::optional<T>& t) {
            if(t) {
                return do_stringify(t.value());
            } else {
                return "nullopt";
            }
        }

        inline constexpr std::size_t max_container_print_items = 1000;

        template<typename T>
        [[nodiscard]]
        std::string stringify_container(const T& container) {
            using std::begin, std::end; // ADL
            std::string str = "[";
            const auto begin_it = begin(container);
            std::size_t count = 0;
            for(auto it = begin_it; it != end(container); it++) {
                if(it != begin_it) {
                    str += ", ";
                }
                str += do_stringify(*it);
                if(++count == max_container_print_items) {
                    str += ", ...";
                    break;
                }
            }
            str += "]";
            return str;
        }

        // I'm going to assume at least one index because is_tuple_like requires index 0 to exist
        template<typename T, size_t... I>
        [[nodiscard]]
        std::string stringify_tuple_like_impl(const T& t, std::index_sequence<I...>) {
            return "[" + (do_stringify(std::get<0>(t)) + ... + (", " + do_stringify(std::get<I + 1>(t)))) + "]";
        }

        template<typename T>
        [[nodiscard]]
        std::string stringify_tuple_like(const T& t) {
            if constexpr(std::tuple_size<T>::value == 0) {
                return "[]";
            } else {
                return stringify_tuple_like_impl(t, std::make_index_sequence<std::tuple_size<T>::value - 1>{});
            }
        }

        template<typename T>
        [[nodiscard]]
        std::string stringify_filesystem_path_like(const T& t) {
            return stringify(t.string());
        }
    }

    template<typename T, typename = void>
    inline constexpr bool has_value_type = false;
    template<typename T>
    inline constexpr bool has_value_type<
        T,
        std::void_t<typename T::value_type>
    > = true;

    template<typename T> inline constexpr bool is_smart_pointer =
        is_specialization<T, std::unique_ptr>
        || is_specialization<T, std::shared_ptr>; // TODO: Handle weak_ptr too?

    template<typename T, typename = void>
    inline constexpr bool can_basic_stringify = false;
    template<typename T>
    inline constexpr bool can_basic_stringify<
        T,
        std::void_t<decltype(stringification::stringify(std::declval<T>()))>
    > = true;

    template<typename T> constexpr bool stringifiable_container();

    template<typename T> inline constexpr bool stringifiable =
        stringification::has_stringifier<T>
        || std::is_convertible_v<T, std::string_view>
        || (std::is_pointer_v<T> || std::is_function_v<T>)
        || std::is_enum_v<T>
        || (stringification::is_tuple_like<T> && stringifiable_container<T>())
        || stringification::is_std_filesystem_path_like<T>
        || (stringification::adl::is_container<T> && stringifiable_container<T>())
        || can_basic_stringify<T>
        || stringification::has_ostream_overload<T>
        #ifdef LIBASSERT_USE_STD_FORMAT
        #if LIBASSERT_STD_VER >= 23
        || std::formattable<T, char> // preferred since this is stricter than the C++20 way of checking
                                     // and makes sure that the C++ community converges on how `std::formatter`
                                     // should be used.
        #elif LIBASSERT_STD_VER == 20
        || requires { std::formatter<T>(); } // fallback for C++20
        #endif
        #endif
        #ifdef LIBASSERT_USE_FMT
        || fmt::is_formattable<T>::value
        #endif
        || stringifiable_container<T>();

    template<typename T, size_t... I> constexpr bool tuple_has_stringifiable_args_core(std::index_sequence<I...>) {
        return sizeof...(I) == 0 || (stringifiable<decltype(std::get<I>(std::declval<T>()))> || ...);
    }

    template<typename T> inline constexpr bool tuple_has_stringifiable_args =
        tuple_has_stringifiable_args_core<T>(std::make_index_sequence<std::tuple_size<T>::value>{});

    template<typename T> constexpr bool stringifiable_container() {
        // TODO: Guard against std::expected....?
        if constexpr(has_value_type<T>) {
            if constexpr(std::is_same_v<typename T::value_type, T>) { // TODO: Reconsider
                return false;
            } else {
                return stringifiable<typename T::value_type>;
            }
        } else if constexpr(std::is_array_v<typename std::remove_reference_t<T>>) { // C arrays
            return stringifiable<decltype(std::declval<T>()[0])>;
        } else if constexpr(stringification::is_tuple_like<T>) {
            return tuple_has_stringifiable_args<T>;
        } else {
            return false;
        }
    }

    // Stringification of some types can result in infinite recursion and subsequent stack-overflow. An example would be
    // a container/range-like type T whose iterator's value_type is also T (such as std::filesystem::path). This is easy
    // enough to detect at compile time, however, there are pathological types in the general case which would be much
    // more difficult to check at compile time.
    // For example, let T be a container whose iterator's value_type = std::vector<T>. This would cause infinite
    // recursion via:
    // do_stringify<T>
    //   -> stringify_container<T>
    //     -> do_stringify<std::vector<T>>
    //       -> stringify_container<std::vector<T>>
    //         -> do_stringify<T>
    //           -> ...
    // Instead of compile-time checks this class and a RAII helper is used at the do_stringify<T> level to detect
    // stringification recursion at runtime.
    class recursion_flag {
        bool the_flag = false;
    public:
        recursion_flag() = default;
        recursion_flag(const recursion_flag&) = delete;
        recursion_flag(recursion_flag&&) = delete;
        recursion_flag& operator=(const recursion_flag&) = delete;
        recursion_flag& operator=(recursion_flag&&) = delete;

        class recursion_canary {
            bool& flag_ref;
        public:
            recursion_canary(bool& flag) : flag_ref(flag) {}
            ~recursion_canary() {
                flag_ref = false;
            }
        };

        recursion_canary set() {
            the_flag = true;
            return recursion_canary(the_flag);
        }

        bool test() const {
            return the_flag;
        }
    };

    template<typename T>
    [[nodiscard]]
    std::string do_stringify(const T& v) {
        // TODO: This is overkill to do for every instantiation of do_stringify (e.g. primitive types could omit this)
        thread_local recursion_flag flag;
        if(flag.test()) { // pathological case detected, fall back to unknown
            return stringification::stringify_unknown<T>();
        }
        auto canary = flag.set();
        // Ordering notes
        // - stringifier first
        // - nullptr before string_view (char*)
        // - other pointers before basic stringify
        // - enum before basic stringify
        // - container before basic stringify (c arrays and decay etc)
        //   - needs to exclude std::filesystem::path
        if constexpr(stringification::has_stringifier<T>) {
            return stringifier<strip<T>>{}.stringify(v);
        } else if constexpr(std::is_same_v<T, std::nullptr_t>) {
            return "nullptr";
        } else if constexpr(std::is_convertible_v<T, std::string_view>) {
            if constexpr(std::is_pointer_v<T>) {
                if(v == nullptr) {
                    return "nullptr";
                }
            }
            #if LIBASSERT_IS_GCC
                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wnonnull"
            #endif
            return stringification::stringify(std::string_view(v));
            #if LIBASSERT_IS_GCC
                #pragma GCC diagnostic pop
            #endif
        } else if constexpr(std::is_pointer_v<T> || std::is_function_v<T>) {
            return stringification::stringify_pointer_value(reinterpret_cast<const void*>(v));
        } else if constexpr(is_smart_pointer<T>) {
            #ifndef LIBASSERT_NO_STRINGIFY_SMART_POINTER_OBJECTS
             if(stringifiable<typename T::element_type>) {
            #else
             if(false) {
            #endif
                return stringification::stringify_smart_ptr(v);
            } else {
                return stringification::stringify_pointer_value(v.get());
            }
        } else if constexpr(std::is_enum_v<T> && !std::is_same_v<T, std::byte>) {
            return stringification::stringify_enum(v);
        } else if constexpr(stringification::is_tuple_like<T>) {
            if constexpr(stringifiable_container<T>()) {
                return stringification::stringify_tuple_like(v);
            } else {
                return stringification::stringify_unknown<T>();
            }
        } else if constexpr(stringification::is_std_filesystem_path_like<T>) {
            return stringification::stringify_filesystem_path_like(v);
        } else if constexpr(stringification::adl::is_container<T>) {
            if constexpr(stringifiable_container<T>()) {
                return stringification::stringify_container(v);
            } else {
                return stringification::stringify_unknown<T>();
            }
        } else if constexpr(can_basic_stringify<T>) {
            return stringification::stringify(v);
        } else if constexpr(stringification::has_ostream_overload<T>) {
            return stringification::stringify_by_ostream(v);
        }
        #ifdef LIBASSERT_USE_STD_FORMAT
        #if LIBASSERT_STD_VER >= 23
        // preferred since this is stricter than the C++20 way of checking and makes sure that the
        // C++ community converges on how `std::formatter` should be used.
        else if constexpr (std::formattable<T, char>) {
            return std::format("{}", v);
        }
        #elif LIBASSERT_STD_VER == 20
        else if constexpr (requires { std::formatter<T>(); }) {
            return std::format("{}", v);
        }
        #endif
        #endif
        #ifdef LIBASSERT_USE_FMT
        else if constexpr(fmt::is_formattable<T>::value) {
            return fmt::format("{}", v);
        }
        #endif
        else {
            return stringification::stringify_unknown<T>();
        }
    }

    // Top-level stringify utility
    template<typename T>
    [[nodiscard]]
    std::string generate_stringification(const T& v) {
        if constexpr(
            stringification::adl::is_container<T>
            && !is_string_type<T>
            && !stringification::is_std_filesystem_path_like<T>
            && stringifiable_container<T>()
        ) {
            return prettify_type(std::string(type_name<T>())) + ": " + do_stringify(v);
        } else if constexpr(stringification::is_tuple_like<T> && stringifiable_container<T>()) {
            return prettify_type(std::string(type_name<T>())) + ": " + do_stringify(v);
        } else if constexpr((std::is_pointer_v<T> && !is_string_type<T>) || is_smart_pointer<T>) {
            return prettify_type(std::string(type_name<T>())) + ": " + do_stringify(v);
        } else if constexpr(is_specialization<T, std::optional>) {
            return prettify_type(std::string(type_name<T>())) + ": " + do_stringify(v);
        } else {
            return do_stringify(v);
        }
    }
}
LIBASSERT_END_NAMESPACE

#endif
