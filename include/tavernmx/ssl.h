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

    struct HttpHeader {
        std::string name;
        std::string content;

        HttpHeader() : name{}, content{} {}

        HttpHeader(const std::string &name, const std::string &content) : name{name}, content{content} {}

        HttpHeader(std::string &&name, std::string &&content) : name{name}, content{content} {}
    };

    [[noreturn]] inline void print_errors_and_exit(const char *message) {
        fprintf(stderr, "%s\n", message);
        ERR_print_errors_fp(stderr);
        exit(1);
    }

    void send_http_request(BIO *bio, const std::string &line, const std::string &host);

    std::string receive_http_message(BIO *bio, std::vector<HttpHeader> &headers_out);
}