//
// Created by Taxes on 2023-11-27.
//
#pragma once

#include <openssl/x509v3.h>
#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "tavernmx/messaging.h"

namespace tavernmx::ssl {
    template<class T>
    struct deleter_of;

    template<>
    struct deleter_of<SSL_CTX> {
        void operator()(SSL_CTX *p) const { SSL_CTX_free(p); }
    };

    template<>
    struct deleter_of<SSL> {
        void operator()(SSL *p) const { SSL_free(p); }
    };

    template<>
    struct deleter_of<BIO> {
        void operator()(BIO *p) const { BIO_free_all(p); }
    };

    template<>
    struct deleter_of<BIO_METHOD> {
        void operator()(BIO_METHOD *p) const { BIO_meth_free(p); }
    };

    template<class OpenSSLType>
    using ssl_unique_ptr = std::unique_ptr<OpenSSLType, deleter_of<OpenSSLType>>;

    inline ssl_unique_ptr<BIO> operator|(ssl_unique_ptr<BIO> lower,
                                         ssl_unique_ptr<BIO> upper) {
        BIO_push(upper.get(), lower.release());
        return upper;
    }

    [[noreturn]] inline void print_errors_and_exit(const char *message) {
        fprintf(stderr, "%s\n", message);
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    void send_message(BIO *bio, const messaging::MessageBlock &block);

    std::optional<messaging::MessageBlock> receive_message(BIO *bio);

    ssl_unique_ptr<BIO> accept_new_tcp_connection(BIO *accept_bio);

    SSL *get_ssl(BIO *bio);

    void verify_certificate(SSL *ssl, bool allow_self_signed, const std::string &expected_hostname);
}