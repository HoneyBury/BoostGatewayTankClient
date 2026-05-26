# Third-Party Assets

This client repository follows the same asset-governance idea as the server
repository, but with a smaller scope.

## Ownership boundary

Server-owned dependencies:

- BoostGateway SDK implementation
- Boost / fmt / spdlog / hiredis / OpenSSL used to build the server and SDK

Client-owned dependencies:

- Qt toolchain and runtime
- The exported BoostGateway SDK package consumed by the client
- Client-local packaging and validation scripts

The client should consume a prepared SDK package instead of rebuilding the
server SDK on every machine.

## Canonical asset layout

```text
third_party/
  sdk-package/
    include/
    lib/
      cmake/
        boost_gateway_sdk/
```

The client CMake helper will prefer this local SDK package automatically.

## Local bootstrap from sibling server repo

Recommended sibling layout:

```text
D:\Program\boost-github\BoostAsioDemo
D:\Program\boost-github\BoostGatewayTankClient
```

Bootstrap the SDK asset into the client repository:

```powershell
.\third_party\bootstrap_from_server.bat D:\Program\boost-github\BoostAsioDemo
```

Linux/macOS:

```bash
./third_party/bootstrap_from_server.sh /path/to/BoostAsioDemo
```

After that, configure the client without relying on the server build tree:

```powershell
cmake -S . -B build\local
cmake --build build\local --config Debug
```

## Remote / internal distribution

Package the client third-party asset set:

```powershell
.\third_party\package.bat
```

Linux/macOS:

```bash
./third_party/package.sh
```

This produces `third_party-client-assets.zip`.

Restore it on another machine:

```powershell
.\third_party\restore_from_archive.bat .\third_party-client-assets.zip
```

Linux/macOS:

```bash
./third_party/restore_from_archive.sh ./third_party-client-assets.zip
```

## Fresh-machine completeness

### Client-only machine

Required:

- CMake 3.24+
- C++20 compiler
- Qt 6 or Qt 5 Widgets
- Restored `third_party/sdk-package/`

Not required:

- OpenSSL headers/libs
- Server source tree
- Server build tree

### Machine rebuilding the SDK from server source

If a machine must rebuild the SDK from the server repository instead of
consuming a packaged SDK, then it must satisfy the server dependency model,
including OpenSSL.

The server currently uses:

```cmake
find_package(OpenSSL REQUIRED)
```

So a fresh machine needs one of:

1. A normal OpenSSL installation discoverable by CMake
2. `OPENSSL_ROOT_DIR`
3. Explicit `OPENSSL_INCLUDE_DIR`, `OPENSSL_SSL_LIBRARY`, and
   `OPENSSL_CRYPTO_LIBRARY`

For maintainability, the preferred workflow is:

1. Build and install the SDK on a prepared server-development machine
2. Package `runtime/sdk-package-prefix/`
3. Restore that package into the client repo's `third_party/sdk-package/`
4. Build the client without rebuilding the server SDK
