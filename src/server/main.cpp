#include <iostream>
#include <string>
#include <vector>
#include <tavernmx/ssl.h>

using namespace tavernmx::ssl;
inline static std::string HOST_NAME{"www.google.com"};
inline static std::string HOST_PORT = HOST_NAME + ":80";

int main() {
    auto ctx = ssl_unique_ptr<SSL_CTX>(SSL_CTX_new(TLS_client_method()));

    auto bio = ssl_unique_ptr<BIO>(BIO_new_connect(HOST_PORT.c_str()));
    if (BIO_do_connect(bio.get()) <= 0) {
        print_errors_and_exit("Error in BIO_do_connect");
    }

    send_http_request(bio.get(), "GET / HTTP/1.1", HOST_NAME);
    std::vector<HttpHeader> headers{};
    std::string response = receive_http_message(bio.get(), headers);
    for (auto &hdr: headers) {
        std::cout << hdr.name << ": " << hdr.content << std::endl;
    }
    std::cout << response << std::endl;
    return 0;
}
