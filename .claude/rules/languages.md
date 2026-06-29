# Language and build constraints

- Production implementation: C++20.
- Expected compiler environment: GCC 15.
- Build system: CMake with Ninja.
- Standalone consumer of an existing VG installation or checkout.
- Do not fetch or build VG as an implicit project step.
- Shell and Python may be used for repository tooling or tests when justified, but not as
  substitutes for the C++ production implementation.
