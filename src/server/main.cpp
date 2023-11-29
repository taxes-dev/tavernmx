#include <iostream>
#include <string>
#include <vector>
#include <tavernmx/ssl.h>
#include <libc.h>

using namespace tavernmx::ssl;

int main() {
    auto ctx = ssl_unique_ptr<SSL_CTX>(SSL_CTX_new(TLS_method()));
    SSL_CTX_set_min_proto_version(ctx.get(), TLS1_2_VERSION);
    if (SSL_CTX_use_certificate_file(ctx.get(), "server-certificate.pem", SSL_FILETYPE_PEM) <= 0) {
        print_errors_and_exit("Error loading server certificate");
    }
    if (SSL_CTX_use_PrivateKey_file(ctx.get(), "server-private-key.pem", SSL_FILETYPE_PEM) <= 0) {
        print_errors_and_exit("Error loading server private key");
    }

    auto accept_bio = ssl_unique_ptr<BIO>(BIO_new_accept("8080"));
    if (BIO_do_accept(accept_bio.get()) != 1) {
        print_errors_and_exit("Error in BIO_do_accept (binding to port 8080)");
    }

    static auto shutdown_the_socket = [fd = BIO_get_fd(accept_bio.get(), nullptr)]() {
        close(static_cast<int>(fd));
    };
    signal(SIGINT, [](int) { shutdown_the_socket(); });

    while (auto bio = accept_new_tcp_connection(accept_bio.get())) {
        // client param: 0 = server, 1 = client
        bio = std::move(bio)
              | ssl_unique_ptr<BIO>(BIO_new_ssl(ctx.get(), 0));
        try {
            std::vector<HttpHeader> headers{};
            std::string request = receive_http_message(bio.get(), headers);
            std::cout << "Got request: " << request.size() << std::endl;
            for (auto &hdr: headers) {
                std::cout << hdr << std::endl;
            }
            std::cout << request << std::endl;
            send_http_response(bio.get(), "Hello, world!\r\n");
        } catch (const std::exception &ex) {
            std::cout << "Worker exited with exception:" << std::endl;
            std::cout << ex.what() << std::endl;
        }
    }
    std::cout << "Exit!" << std::endl;
}

