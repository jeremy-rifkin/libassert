# Contributing

Welcome, thank you for your interest in the project!

## Getting started

Contributions are always welcome. If you have not already, consider joining the community discord
(linked in the README). There is discussion about library development there as well as a development
roadmap. Github issues are also a good place to start.

I'm happy to merge fixes, improvements, and features as well as help with getting pull requests
(PRs) over the finish line. That being said, I can't merge stylistic changes,
premature-optimizations, or micro-optimizations.

When contributing, please try to match the current code style in the codebase. Style doesn't matter
too much ultimately but consistency within a codebase is important.

## Local development

The easiest way to develop locally is to run `make build` which will handle cmake invocation and
build in `build/`. Alternatively you can manually run `cmake ..`, along with any cmake
configurations you desire in a build folder. Then run `make -j`, `ninja`, or
`msbuild libassert.sln`.

Some useful configurations:
- `-DCMAKE_BUILD_TYPE=Debug|Release|RelWithDebInfo`: Build in debug / release / etc.
- `-DBUILD_SHARED_LIBS=On`: Build shared library
- `-DLIBSANITIZER_BUILD=On`: Turn on sanitizers
- `-DLIBASSERT_BUILD_TESTING=On`: Build test and demo programs

## Testing

Run `make test`, `ninja test`, or `msbuild RUN_TESTS.vcxproj` to run tests. Unfortunately testing
for this project is not in a great state at the moment and while there is some unit testing it
relies heavily on integration testing that is hard to maintain.
