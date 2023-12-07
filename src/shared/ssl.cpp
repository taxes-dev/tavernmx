//
// Created by Taxes on 2023-11-27.
//
#include <iostream>
#include <string>
#include <vector>
#include <tavernmx/ssl.h>

using namespace std::string_literals;

namespace {
    // receive buffer size here roughly matches typical ethernet MTU
    inline const size_t BUFFER_SIZE = 1500;

    size_t receive_bytes(BIO *bio, unsigned char *buffer, size_t bufsize) {
        retry:
        int32_t len = BIO_read(bio, buffer, static_cast<int32_t>(bufsize));
        if (BIO_should_retry(bio)) {
            BIO_wait(bio, time(nullptr) + 3, 250);
            goto retry;
        } else if (len < 0) {
            tavernmx::ssl::print_errors_and_exit("error in BIO_read");
        } else if (len > 0) {
            return static_cast<size_t>(len);
        } else {
            tavernmx::ssl::print_errors_and_exit("empty BIO_read");
        }
    }
}

namespace tavernmx::ssl {

    void send_message(BIO *bio, const messaging::MessageBlock &block) {
        auto block_data = messaging::pack_block(block);
        int written = BIO_write(bio, block_data.data(), static_cast<int32_t>(block_data.size()));
        if (written < 0) {
            print_errors_and_exit("BIO_write failed");
        }
        BIO_flush(bio);
    }

    std::optional<messaging::MessageBlock> receive_message(BIO *bio) {
        unsigned char buffer[BUFFER_SIZE];
        size_t rcvd = receive_bytes(bio, buffer, sizeof(buffer));
        if (rcvd == 0) {
            return {};
        }

        messaging::MessageBlock block{};
        size_t applied = messaging::apply_buffer_to_block(buffer, rcvd, block);
        while (applied > 0 && applied < block.payload_size) {
            rcvd = receive_bytes(bio, buffer, sizeof(buffer));
            applied += messaging::apply_buffer_to_block(buffer, rcvd, block, applied);
        }

        if (block.payload_size == 0) {
            return {};
        }
        return block;
    }

    ssl_unique_ptr<BIO> accept_new_tcp_connection(BIO *accept_bio) {
        if (BIO_do_accept(accept_bio) <= 0) {
            return nullptr;
        }
        return ssl_unique_ptr<BIO>(BIO_pop(accept_bio));
    }

    SSL *get_ssl(BIO *bio) {
        SSL *ssl = nullptr;
        BIO_get_ssl(bio, &ssl);
        if (ssl == nullptr) {
            print_errors_and_exit("Error in BIO_get_ssl");
        }
        return ssl;
    }

    void verify_certificate(SSL *ssl, bool allow_self_signed, const std::string &expected_hostname) {
        long err = SSL_get_verify_result(ssl);
        if (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN || err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
            const char *msg = X509_verify_cert_error_string(err);
            std::cerr << "Self-signed certificate encountered: " << err << " " << msg << std::endl;
            if (!allow_self_signed) {
                exit(1);
            }
        } else if (err != X509_V_OK) {
            const char *msg = X509_verify_cert_error_string(err);
            std::cerr << "SSL_get_verify_result: " << err << " " << msg << std::endl;
            exit(1);
        }
        X509 *cert = SSL_get_peer_certificate(ssl);
        if (cert == nullptr) {
            std::cerr << "SSL_get_peer_certificate: No certificate was presented by the server" << std::endl;
            exit(1);
        }
        if (X509_check_host(cert, expected_hostname.data(), expected_hostname.size(), 0, nullptr) != 1) {
            std::cerr << "X509_check_host: Certificate verification error: Hostname mismatch" << std::endl;
            exit(1);
        }
    }
}