# tavernmx

## CMake configuration

```shell
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=<vcpkg root>/scripts/buildsystems/vcpkg.cmake -G Ninja -S . -B ./cmake-build-debug
```

## Creating server certificates

```shell
openssl ecparam -genkey -name prime256v1 -noout -out server-private-key.pem
openssl ec -in server-private-key.pem -pubout -out server-public-key.pem
openssl req -new -x509 -sha256 -key server-private-key.pem -subj "/CN=<hostname>" -out server-certificate.pem
```