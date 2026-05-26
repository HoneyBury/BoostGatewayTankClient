# Third-Party Assets

This directory mirrors the server repository's offline/internal-network asset
management model, but the client keeps a much smaller asset surface.

The client consumes:

- a **BoostGateway SDK package** copied from the server repository or an
  internal artifact mirror
- external **Qt toolchain/runtime** installed on the local machine

Unlike the server repository, the client does **not** vendor Boost, fmt,
spdlog, or OpenSSL source trees. Those remain server-owned dependencies.

## Current policy

- Treat the BoostGateway SDK package as a third-party client asset.
- Prefer a copied SDK install prefix under `third_party/sdk-package/` for
  offline and fresh-machine client builds.
- Keep Qt as an external toolchain dependency. Do not vendor Qt into this
  repository.
- Keep OpenSSL as a **server-owned prerequisite**. The client does not require
  OpenSSL when it builds only against a prebuilt SDK package.

## Asset layout

The canonical extracted SDK layout is:

```text
third_party/
  sdk-package/
    include/
    lib/
      cmake/
        boost_gateway_sdk/
```

The client CMake helper will automatically look there before falling back to a
live sibling `BoostAsioDemo` checkout.

## Local bootstrap from sibling server repo

Recommended sibling layout:

```text
D:\Program\boost-github\BoostAsioDemo
D:\Program\boost-github\BoostGatewayTankClient
```

Bootstrap from an existing server SDK package:

```powershell
.\third_party\bootstrap_from_server.bat D:\Program\boost-github\BoostAsioDemo
```

Linux/macOS:

```bash
./third_party/bootstrap_from_server.sh /path/to/BoostAsioDemo
```

The script copies `runtime/sdk-package-prefix/` from the server repo into
`third_party/sdk-package/`.

## Remote / internal distribution

Create a distributable client asset archive:

```powershell
.\third_party\package.bat
```

Linux/macOS:

```bash
./third_party/package.sh
```

This produces `third_party-client-assets.zip` in the repository root.

Restore it on another machine:

```powershell
.\third_party\restore_from_archive.bat .\third_party-client-assets.zip
```

Linux/macOS:

```bash
./third_party/restore_from_archive.sh ./third_party-client-assets.zip
```

## Fresh-machine dependency completeness

### Client-only build machine

To build the client from a prebuilt SDK package, the machine needs:

- CMake 3.24+
- C++20 compiler
- Qt 6 or Qt 5 Widgets
- `third_party/sdk-package/` restored from a trusted SDK artifact

OpenSSL is **not** required on this machine for the client build path.

### Machine rebuilding the SDK from the server repo

If you must rebuild the SDK from `BoostAsioDemo` source on a fresh machine,
that machine must satisfy the **server** prerequisites, including OpenSSL.

The server repo currently discovers OpenSSL via `find_package(OpenSSL REQUIRED)`.
That means a fresh machine must provide one of:

1. A system OpenSSL installation discoverable by CMake
2. `OPENSSL_ROOT_DIR` pointing to a valid OpenSSL installation
3. Explicit `OPENSSL_INCLUDE_DIR`, `OPENSSL_SSL_LIBRARY`, and
   `OPENSSL_CRYPTO_LIBRARY`

Operationally, the safer path is:

1. Build and install the SDK on a prepared server-development machine
2. Package `runtime/sdk-package-prefix/`
3. Distribute that package to client build machines through
   `third_party-client-assets.zip`

That keeps OpenSSL ownership on the server side and avoids leaking server
toolchain requirements into client-only environments.

