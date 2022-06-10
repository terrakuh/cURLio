# Change Log

## [v0.3.3]
### Added
- Convenience function to await all headers
- Simple example

### Changed
- Use system_error of Boost instead of `std::system_error`

### Fixed
- Getting stuck in multi threading mode
- Completion handlers with ambiguous signature
- Header collector synchronization

## [v0.3.2]
### Fixed
- Post completion handlers for execution instead of direct invocation

## [v0.3.1]
### Fixed
- Remove easy handle before destruction

## [v0.3] - 2022-04-25
### Added
- Waiting for headers
- Move operations for `Request` and `Response`
- Synchronize `Session` with `boost::asio::strand`

### Changed
- Function `async_wait()` renamed to `async_await_completion()`
- Split logic into `Request` and `Response` classes

## [v0.2]
### Added
- JSON writer and reader
- Easier HTTP field setting option
- Saving cookies to file
- Quick way to construct a string from form parameters
- Quick way to read all the content
- Logging definition `CURLIO_ENABLE_LOGGING` for debugging the library

### Fixed
- Cookie share support

[Unreleased]: https://github.com/terrakuh/curlio/compare/v0.3.3..dev
[v0.3.3]: https://github.com/terrakuh/curlio/compare/v0.3.2..v0.3.3
[v0.3.2]: https://github.com/terrakuh/curlio/compare/v0.3.1..v0.3.2
[v0.3.1]: https://github.com/terrakuh/curlio/compare/v0.3..v0.3.1
[v0.3]: https://github.com/terrakuh/curlio/compare/v0.2..v0.3
[v0.2]: https://github.com/terrakuh/curlio/compare/v0.1..v0.2
