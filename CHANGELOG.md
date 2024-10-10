# Change Log

<h2><a href="https://github.com/terrakuh/curlio/compare/v0.5.0..v0.6.0">v0.6.0</a> - 2024-10-10</h2>

### Added
- Per-operation cancellation support
- More error messages and propagation

### Fixed
- Re-registering of fast handles

<h2><a href="https://github.com/terrakuh/curlio/compare/v0.4..v0.5.0">v0.5.0</a> - 2024-08-04</h2>

### Changed
- Less relying on pointers
- CMake target name `cURLio::cURLio`

### Fixed
- Fix segfaults on kick-starting new requests
- Fix CURL deprecation warnings
- Various bugs

<h2><a href="https://github.com/terrakuh/curlio/compare/v0.3.3..v0.4">v0.4</a> - 2022-11-27</h2>

### Changed
- Implementation

<h2><a href="https://github.com/terrakuh/curlio/compare/v0.3.2..v0.3.3">v0.3.3</a> - 2022-06-10</h2>

### Added
- Convenience function to await all headers
- Simple example

### Changed
- Use system_error of Boost instead of `std::system_error`

### Fixed
- Getting stuck in multi threading mode
- Completion handlers with ambiguous signature
- Header collector synchronization

<h2><a href="https://github.com/terrakuh/curlio/compare/v0.3.1..v0.3.2">v0.3.2</a></h2>

### Fixed
- Post completion handlers for execution instead of direct invocation

<h2><a href="https://github.com/terrakuh/curlio/compare/v0.3..v0.3.1">v0.3.1</a></h2>

### Fixed
- Remove easy handle before destruction

<h2><a href="https://github.com/terrakuh/curlio/compare/v0.2..v0.3">v0.3</a> - 2022-04-25</h2>

### Added
- Waiting for headers
- Move operations for `Request` and `Response`
- Synchronize `Session` with `boost::asio::strand`

### Changed
- Function `async_wait()` renamed to `async_await_completion()`
- Split logic into `Request` and `Response` classes

<h2><a href="https://github.com/terrakuh/curlio/compare/v0.1..v0.2">v0.2</a></h2>

### Added
- JSON writer and reader
- Easier HTTP field setting option
- Saving cookies to file
- Quick way to construct a string from form parameters
- Quick way to read all the content
- Logging definition `CURLIO_ENABLE_LOGGING` for debugging the library

### Fixed
- Cookie share support
