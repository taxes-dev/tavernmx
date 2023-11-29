#include <iostream>
#include <string>
#include <vector>
#include <tavernmx/ssl.h>

using namespace tavernmx::ssl;
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
    //BIO_set_nbio(bio.get(), 1);
    //if (BIO_do_connect_retry(bio.get(), 3, 100) != 1) {
    bio = std::move(bio) | ssl_unique_ptr<BIO>(BIO_new_ssl(ctx.get(), 1));
    if (BIO_do_connect(bio.get()) != 1) {
        print_errors_and_exit("Error in BIO_do_connect");
    }
    SSL_set_tlsext_host_name(get_ssl(bio.get()), HOST_NAME.c_str());
    SSL_set1_host(get_ssl(bio.get()), HOST_NAME.c_str());
    //SSL_set_verify(get_ssl(ssl_bio.get()), SSL_VERIFY_NONE, nullptr);
    if (BIO_do_handshake(bio.get()) <= 0) {
        print_errors_and_exit("Error in TLS handshake");
    }
    verify_the_certificate(get_ssl(bio.get()), false, HOST_NAME);

    std::cout << "Sending request" << std::endl;
    send_http_request(bio.get(), "GET / HTTP/1.1"s, HOST_NAME);
    std::vector<HttpHeader> headers{};
    std::string response = receive_http_message(bio.get(), headers);
    for (auto &hdr: headers) {
        std::cout << hdr << std::endl;
    }
    std::cout << response << std::endl;
    return 0;
}