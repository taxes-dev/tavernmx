# tavernmx

`tavernmx` is a simple chat room server with accompanying graphical client program. There are three project targets:

* `tavernmx` - Server daemon.
* `tavernmx-client` - Client application.
* `tavernmx-shared` - Static library of shared code between server and client.

![](example.png)

## Building

Building requires:
* [CMake](https://cmake.org/) 3.26+
* A C++20-capable compiler
* [vcpkg](https://vcpkg.io/en/)

Tested OS/compilers:
* Visual Studio 2022 on Windows 11 (x64)
* CLion 2023 on macOS Sonoma (arm64), with Xcode 14.3+
* Makefiles on Ubuntu 22.04 (x64), with gcc 11+

### CMake configuration

CLion and Visual Studio should be able to configure the project automatically. Make sure vcpkg is available and integrated with the IDE.

If you need to configure manually, specify the vcpkg toolchain file. Example:

```shell
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake -G Ninja -S . -B ./cmake-build-debug
```

### Dependencies

The project uses several external dependencies, but they should be automatically resolved by vcpkg.

* [OpenSSL 3.3.1](https://www.openssl.org/)
* [nlohmann-json 3.11.3](https://json.nlohmann.me/)
* [spdlog 1.14.1](https://github.com/gabime/spdlog)
* [SDL 2.30.5](https://www.libsdl.org/) - client only
* [Dear ImGui 1.90.7](https://github.com/ocornut/imgui) - client only
* [BS::thread_pool 4.1.0](https://github.com/bshoshany/thread-pool) - server only
* [Catch2 3.6.0](https://github.com/catchorg/Catch2) - tests only

## Running

### Configuration

The server requires a configuration file called `server-config.json`, and the client requires a configuration file called `client-config.json`. These files must be present in the working directory. Examples are included in the repository root.

### Creating server certificates

The server and client communicate over TLS, which requires creating a set of custom certificate files. Example shown below.

On Windows, the easiest way to achieve this is with [Git Bash's](https://gitforwindows.org/) built-in openssl.

Replace `<hostname>` below with the host name you plan to connect to (`localhost` works for local testing).

```shell
openssl ecparam -genkey -name prime256v1 -noout -out server-private-key.pem
openssl ec -in server-private-key.pem -pubout -out server-public-key.pem
openssl req -new -x509 -sha256 -key server-private-key.pem -subj "/CN=<hostname>" -out server-certificate.pem
```

### Start the server

For a quick start, once the certificates have been created, you can start the server by running the `tavernmx` binary. If you used the suggested file names above and run it from the repository root, the example configuration should get everything running.

### Start the client

Once the server is running, you can start one or more instances of `tavernmx-client`, also using the repository root as the working directory. Make sure the host and port shown in the UI match what the server is using and click the 'Connect' button to get started.

## License

Open source. Distributed under an [MIT license](LICENSE.md).
