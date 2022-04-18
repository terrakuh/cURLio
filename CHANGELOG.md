# Change Log

## [Unreleased]
### Added
- Waiting for headers

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

[Unreleased]: https://github.com/terrakuh/curlio/compare/v0.2..dev
[v0.2]: https://github.com/terrakuh/curlio/compare/v0.1..v0.2
