#include <iostream>
#include <string>
#include <vector>
#include <tavernmx/ssl.h>
#include <libc.h>

using namespace tavernmx::ssl;
using namespace tavernmx::messaging;

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
    BIO_set_nbio(accept_bio.get(), 1);
    if (BIO_do_accept(accept_bio.get()) != 1) {
        print_errors_and_exit("Error in BIO_do_accept (binding to port 8080)");
    }

    static auto shutdown_the_socket = [fd = BIO_get_fd(accept_bio.get(), nullptr)]() {
        close(static_cast<int>(fd));
    };
    signal(SIGINT, [](int) { shutdown_the_socket(); });
    signal(SIGPIPE, SIG_IGN);

    while (auto bio = accept_new_tcp_connection(accept_bio.get())) {
        // client param: 0 = server, 1 = client
        bio = std::move(bio)
              | ssl_unique_ptr<BIO>(BIO_new_ssl(ctx.get(), 0));
        try {
            auto block = receive_message(bio.get());
            if (block.has_value())
            {
                std::cout << "Got message block: " << block->payload_size << std::endl;
                for (unsigned char c : block->payload)
                {
                    std::cout << "Byte: " << static_cast<int32_t>(c) << std::endl;
                }
            }
            else
            {
                std::cout << "No message block found." << std::endl;
            }
        } catch (const std::exception &ex) {
            std::cout << "Worker exited with exception:" << std::endl;
            std::cout << ex.what() << std::endl;
        }
    }
    std::cout << "Exit!" << std::endl;
}

