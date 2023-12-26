#include <chrono>
#include <string>
#include <thread>
#include <vector>
#include <openssl/x509v3.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/pem.h>
#include "tavernmx/ssl.h"

using namespace tavernmx::messaging;
using namespace std::string_literals;

namespace {
    // receive buffer size here roughly matches typical ethernet MTU
    constexpr size_t BUFFER_SIZE = 1500;

    /**
     * @brief Returns an exception containing current queued openssl error messages.
     * @param message Base exception message.
     * @return tavernmx::ssl::SslError
     */
    tavernmx::ssl::SslError ssl_errors_to_exception(const char* message) {
        std::string msg{message};
        while (const unsigned long err = ERR_get_error() != 0) {
            char buffer[256];
            ERR_error_string_n(err, buffer, sizeof(buffer));
            msg += ", ";
            msg += buffer;
        }
        return tavernmx::ssl::SslError{msg};
    }

    /**
     * @brief Attempt to read bytes from the given openssl BIO.
     * @param ssl SSL
     * @param bio BIO
     * @param buffer buffer to read into
     * @param bufsize size of \p buffer
     * @return the number of bytes read
     * @throws tavernmx::ssl::SslError if there's an underlying socket error
     */
    size_t receive_bytes(SSL* ssl, BIO* bio, unsigned char* buffer, size_t bufsize) {
        while ((SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) != SSL_RECEIVED_SHUTDOWN) {
            ERR_clear_error();
            const int32_t len = BIO_read(bio, buffer, static_cast<int32_t>(bufsize));
            if (BIO_should_retry(bio)) {
                BIO_wait(bio, time(nullptr) + std::min(tavernmx::ssl::SSL_RETRY_MILLISECONDS / 1000ll, 1ll),
                         tavernmx::ssl::SSL_NAP_MILLISECONDS);
                // BIO_wait pushes a timeout error into the queue
                ERR_clear_error();
                break;
            }
            if (len > 0) {
                return static_cast<size_t>(len);
            }
            if (SSL_get_error(ssl, len) == SSL_ERROR_ZERO_RETURN) {
                break;
            }
            throw ssl_errors_to_exception("receive_bytes read error");
        }
        return 0;
    }
}

namespace tavernmx::ssl {
    void send_message(BIO* bio, const MessageBlock& block) {
        const std::vector<unsigned char> block_data = pack_block(block);
        if (BIO_write(bio, block_data.data(), static_cast<int32_t>(block_data.size())) < 0) {
            throw ssl_errors_to_exception("send_message BIO_write failed");
        }
        BIO_flush(bio);
    }

    std::optional<MessageBlock> receive_message(BIO* bio) {
        SSL* ssl = get_ssl(bio);
        unsigned char buffer[BUFFER_SIZE];
        size_t rcvd = receive_bytes(ssl, bio, buffer, sizeof(buffer));
        if (rcvd == 0) {
            return {};
        }

        MessageBlock block{};
        size_t applied = apply_buffer_to_block(buffer, rcvd, block);
        while (applied > 0 && applied < block.payload_size) {
            rcvd = receive_bytes(ssl, bio, buffer, sizeof(buffer));
            applied += apply_buffer_to_block(buffer, rcvd, block, applied);
        }

        if (block.payload_size == 0) {
            return {};
        }
        return block;
    }

    ssl_unique_ptr<BIO> accept_new_tcp_connection(BIO* accept_bio) {
        if (BIO_do_accept(accept_bio) <= 0) {
            return nullptr;
        }
        return ssl_unique_ptr<BIO>(BIO_pop(accept_bio));
    }

    SSL* get_ssl(BIO* bio) {
        SSL* ssl = nullptr;
        BIO_get_ssl(bio, &ssl);
        if (ssl == nullptr) {
            throw ssl_errors_to_exception("BIO_get_ssl failed");
        }
        return ssl;
    }

    void verify_certificate(SSL* ssl, bool allow_self_signed, const std::string& expected_hostname) {
        const long err = SSL_get_verify_result(ssl);
        if (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN || err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
            if (!allow_self_signed) {
                const char* msg = X509_verify_cert_error_string(err);
                throw SslError{msg};
            }
        } else if (err != X509_V_OK) {
            const char* msg = X509_verify_cert_error_string(err);
            throw SslError{msg};
        }
        X509* cert = SSL_get_peer_certificate(ssl);
        if (cert == nullptr) {
            throw SslError{"SSL_get_peer_certificate: No certificate was presented by the server"};
        }
        if (X509_check_host(cert, expected_hostname.data(), expected_hostname.size(), 0, nullptr) != 1) {
            throw SslError{"X509_check_host: Hostname mismatch"};
        }
    }

    bool is_connected(BIO* bio) {
        if (bio == nullptr) {
            return false;
        }
        try {
            SSL* ssl = get_ssl(bio);
            if ((SSL_get_shutdown(ssl) & SSL_RECEIVED_SHUTDOWN) == SSL_RECEIVED_SHUTDOWN) {
                return false;
            }
        } catch (SslError&) {
            return false;
        }
        return true;
    }
}
