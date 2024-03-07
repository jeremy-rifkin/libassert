#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#define LIBASSERT_PREFIX_ASSERTIONS
#include <libassert/assert.hpp>

#include <cstdint>

class LibassertMatcher : public Catch::Matchers::MatcherBase<int> {
    std::string message;

public:
    LibassertMatcher() = default;
    LibassertMatcher(std::string message_) : message(std::move(message_)) {}

    bool match(const int&) const override {
        return message.empty();
    }

    virtual std::string describe() const override {
        return message;
    }
};

// #define CHECK(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); CHECK_THAT(1, LibassertMatcher()); } catch(std::exception& e) { CHECK_THAT(false, LibassertMatcher(e.what())); } } while(false)
// #define REQUIRE(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); REQUIRE_THAT(1, LibassertMatcher()); } catch(std::exception& e) { REQUIRE_THAT(false, LibassertMatcher(e.what())); } } while(false)

#define ASSERT(...) do { try { LIBASSERT_ASSERT(__VA_ARGS__); REQUIRE_THAT(1, LibassertMatcher()); } catch(std::exception& e) { REQUIRE_THAT(false, LibassertMatcher(e.what())); } } while(false)

void failure_handler(const libassert::assertion_info& info) {
    std::string message = "";
    throw std::runtime_error(info.to_string(0));
}

auto pre_main = [] () {
    libassert::set_failure_handler(failure_handler);
    return 1;
} ();

uint32_t factorial( uint32_t number ) {
    return number <= 1 ? number : factorial(number-1) * number;
}

TEST_CASE( "Factorials are computed", "[factorial]" ) {
    REQUIRE( factorial( 1) == 1 );
    REQUIRE( factorial( 2) == 2 );
    LIBASSERT_ASSERT(22 == 40, "foobar");
    REQUIRE( factorial( 3) == 60 );
    REQUIRE( factorial(10) == 3'628'800 );
}
