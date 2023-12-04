#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <tavernmx/ssl.h>

using namespace tavernmx::ssl;
using namespace tavernmx::messaging;
using namespace std::string_literals;
namespace {
    inline const std::string HOST_NAME{"localhost"};
    inline const std::string HOST_PORT = HOST_NAME + ":8080";
}

int main() {
    auto ctx = ssl_unique_ptr<SSL_CTX>(SSL_CTX_new(TLS_client_method()));
    SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION);
    if (SSL_CTX_set_default_verify_paths(ctx.get()) != 1) {
        print_errors_and_exit("Error loading trust store");
    }
    if (SSL_CTX_load_verify_locations(ctx.get(), "server-certificate.pem", nullptr) != 1)
    {
        print_errors_and_exit("Error loading server cert");
    }

    auto bio = ssl_unique_ptr<BIO>(BIO_new_connect(HOST_PORT.c_str()));
    BIO_set_nbio(bio.get(), 1);
    if (BIO_do_connect_retry(bio.get(), 3, 100) != 1) {
    //if (BIO_do_connect(bio.get()) != 1) {
        print_errors_and_exit("Error in BIO_do_connect");
    }
    bio = std::move(bio) | ssl_unique_ptr<BIO>(BIO_new_ssl(ctx.get(), 1));
    SSL_set_tlsext_host_name(get_ssl(bio.get()), HOST_NAME.c_str());
    SSL_set1_host(get_ssl(bio.get()), HOST_NAME.c_str());
    //SSL_set_verify(get_ssl(ssl_bio.get()), SSL_VERIFY_NONE, nullptr);
    handshake_retry:
    if (BIO_do_handshake(bio.get()) <= 0) {
        if (BIO_should_retry(bio.get()))
        {
            goto handshake_retry;
        }
        print_errors_and_exit("Error in TLS handshake");
    }
    verify_the_certificate(get_ssl(bio.get()), false, HOST_NAME);

    std::cout << "Sending request" << std::endl;
    MessageBlock block{};
    block.payload.resize(2400);
    int32_t count = 0;
    std::transform(std::begin(block.payload), std::end(block.payload), std::begin(block.payload), [&count](unsigned char c) {
        count++;
        return static_cast<unsigned char>(count % 255);
    });
    block.block_size = static_cast<int32_t>(block.payload.size());
    send_message(bio.get(), block);

    return 0;
}