#pragma once

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "tavernmx/messaging.h"

namespace tavernmx::ssl {
    /// Used by BIO_new_ssl() to indicate a client socket
    constexpr long NEWSSL_CLIENT = 1;
    /// Used by BIO_new_ssl() to indicate a server socket
    constexpr long NEWSSL_SERVER = 0;
    /// Matches the type of std::chrono::milliseconds, min 45 bits
    using Milliseconds = long long;
    /// Number of milliseconds to wait between SSL retries
    constexpr Milliseconds SSL_RETRY_MILLISECONDS = 50;
    /// Number of milliseconds to wait for an expected response
    constexpr Milliseconds SSL_TIMEOUT_MILLISECONDS = 3000;

    /**
     * @brief Base template for openssl deleters.
     * @tparam T an openssl pointer type
     */
    template<class T>
    struct deleter_of;

    /**
     * @brief openssl deleter specialization for SSL_CTX.
     */
    template<>
    struct deleter_of<SSL_CTX> {
        void operator()(SSL_CTX* p) const { SSL_CTX_free(p); }
    };

    /**
     * @brief openssl deleter specialization for SSL.
     */
    template<>
    struct deleter_of<SSL> {
        void operator()(SSL* p) const { SSL_free(p); }
    };

    /**
     * @brief openssl deleter specialization for BIO.
     */
    template<>
    struct deleter_of<BIO> {
        void operator()(BIO* p) const { BIO_free_all(p); }
    };

    /**
     * @brief openssl deleter specialization for BIO_METHOD.
     */
    template<>
    struct deleter_of<BIO_METHOD> {
        void operator()(BIO_METHOD* p) const { BIO_meth_free(p); }
    };

    /**
     * @brief Alias for a std::unique_ptr to own one of the openssl pointers.
     * @tparam OpenSSLType the openssl pointer type
     */
    template<class OpenSSLType>
    using ssl_unique_ptr = std::unique_ptr<OpenSSLType, deleter_of<OpenSSLType>>;

    /**
     * @brief Operator shorthand to push a new BIO onto the stack.
     * @param lower the existing BIO stack to modify
     * @param upper the new BIO to push onto the stack
     * @return the new BIO stack
     */
    inline ssl_unique_ptr<BIO> operator|(ssl_unique_ptr<BIO> lower,
                                         ssl_unique_ptr<BIO> upper) {
        BIO_push(upper.get(), lower.release());
        return upper;
    }

    /**
     * @brief Exception representing low-level openssl errors. 
     */
    class SslError : public std::exception {
    public:
        /**
         * @brief Create a new SslError. 
         * @param what description of the error
         */
        explicit SslError(std::string what) noexcept: what_str{std::move(what)} {
        };

        /**
         * @brief Create a new SslError. 
         * @param what description of the error 
         */
        explicit SslError(const char* what) noexcept: what_str{what} {
        };


        /**
         * @brief Returns an explanatory string.
         * @return pointer to a NULL-terminated string
         */
        const char* what() const noexcept override { return this->what_str.c_str(); }

    private:
        std::string what_str{};
    };

    /**
     * @brief Sends a message over the SSL socket.
     * @param bio pointer to BIO
     * @param block block of data to send
     * @throws SslError if any network errors occur
     */
    void send_message(BIO* bio, const messaging::MessageBlock& block);

    /**
     * @brief Attempts to read a message from the SSL socket.
     * @param bio pointer to BIO
     * @return a tavernmx::messaging::MessageBlock if a well-formed message block was read, otherwise empty
     * @throws SslError if any network errors occur
     * @note Automatically sleeps for SSL_RETRY_MILLISECONDS when no data is waiting in order
     * to prevent tight loops.
     */
    std::optional<messaging::MessageBlock> receive_message(BIO* bio);

    /**
     * @brief Blocks until a new client connections to \p accept_bio.
     * @param accept_bio pointer to the server's accept BIO
     * @return pointer to a new client connection BIO
     * @throws SslError if any network errors occur
     */
    ssl_unique_ptr<BIO> accept_new_tcp_connection(BIO* accept_bio);

    /**
     * @brief Gets a pointer to SSL from \p bio.
     * @param bio pointer to BIO
     * @return pointer to SSL
     * @throws SslError if the operation is unsuccessful
     */
    SSL* get_ssl(BIO* bio);

    /**
     * @brief Validates the certificate attached to the \p ssl connection.
     * @param ssl pointer to SSL
     * @param allow_self_signed if true, self-signed certificates will be permitted
     * @param expected_hostname the host name for the connection to match against the SAN
     * @throws SslError if the certificate does not pass validation
     */
    void verify_certificate(SSL* ssl, bool allow_self_signed, const std::string& expected_hostname);

    /**
     * @brief Check if the \p bio is connected.
     * @param bio pointer to BIO
     * @return true if connected, otherwise false
     */
    bool is_connected(BIO* bio);

}
