//
// Created by Taxes on 2023-11-27.
//
#include <iostream>
#include <string>
#include <vector>
#include <tavernmx/ssl.h>

using namespace std::string_literals;

namespace {
    inline const std::string NL{"\r\n"};
    inline const std::string NL2 = NL + NL;

    std::string receive_some_data(BIO *bio) {
        char buffer[1500];
        retry:
        int len = BIO_read(bio, buffer, sizeof(buffer));
        if (len < 0) {
            tavernmx::ssl::print_errors_and_exit("error in BIO_read");
        } else if (len > 0) {
            return {buffer, static_cast<size_t>(len)};
        } else if (BIO_should_retry(bio)) {
            goto retry;
        } else {
            tavernmx::ssl::print_errors_and_exit("empty BIO_read");
        }
    }

    std::vector<std::string> split_headers(const std::string &text) {
        std::vector<std::string> lines;
        size_t pos = 0;
        auto nlpos = text.find(NL);
        while (nlpos != std::string::npos) {
            lines.emplace_back(&text[pos], &text[nlpos]);
            pos = nlpos + 2;
            nlpos = text.find(NL, pos);
        }
        return lines;
    }

}

namespace tavernmx::ssl {
    void send_http_request(BIO *bio, const std::string &line, const std::string &host) {
        std::string request = line + NL;
        request += "Host: " + host + NL2;

        int written = BIO_write(bio, request.data(), static_cast<int>(request.size()));
        if (written < 0)
        {
            print_errors_and_exit("BIO_write failed");
        }
        BIO_flush(bio);
    }

    std::string receive_http_message(BIO *bio, std::vector<HttpHeader> &headers_out) {
        std::string headers = receive_some_data(bio);
        auto end_of_headers = headers.find(NL2);
        while (end_of_headers == std::string::npos) {
            headers += receive_some_data(bio);
            end_of_headers = headers.find(NL2);
        }
        std::string body = headers.substr(end_of_headers + NL2.size());
        headers.resize(end_of_headers);
        size_t content_length = 0;

        auto all_headers = split_headers(headers);
        headers_out.reserve(all_headers.size());
        for (const std::string &line: all_headers) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto &hdr = headers_out.emplace_back(
                        line.substr(0, colon),
                        line.substr(colon + 2)
                );
                if (hdr.name == "Content-Length"s) {
                    content_length = std::stoul(hdr.content);
                }
            }
        }
        while (body.size() < content_length) {
            body += receive_some_data(bio);
        }
        return body;
    }

    ssl_unique_ptr<BIO> accept_new_tcp_connection(BIO *accept_bio) {
        if (BIO_do_accept(accept_bio) <= 0) {
            return nullptr;
        }
        return ssl_unique_ptr<BIO>(BIO_pop(accept_bio));
    }

    void send_http_response(BIO *bio, const std::string &body) {
        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        response += "\r\n";

        BIO_write(bio, response.data(), static_cast<int>(response.size()));
        BIO_write(bio, body.data(), static_cast<int>(body.size()));
        BIO_flush(bio);
    }

    SSL *get_ssl(BIO* bio)
    {
        SSL *ssl = nullptr;
        BIO_get_ssl(bio, &ssl);
        if (ssl == nullptr)
        {
            print_errors_and_exit("Error in BIO_get_ssl");
        }
        return ssl;
    }

    void verify_the_certificate(SSL *ssl, bool allow_self_signed, const std::string& expected_hostname){
        long err = SSL_get_verify_result(ssl);
        if (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN || err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
        {
            const char *msg = X509_verify_cert_error_string(err);
            std::cerr << "Self-signed certificate encountered: " << err << " " << msg << std::endl;
            if (!allow_self_signed)
            {
                exit(1);
            }
        }
        else if (err != X509_V_OK)
        {
            const char *msg = X509_verify_cert_error_string(err);
            std::cerr << "SSL_get_verify_result: " << err << " " << msg << std::endl;
            exit(1);
        }
        X509 *cert = SSL_get_peer_certificate(ssl);
        if (cert == nullptr)
        {
            std::cerr << "SSL_get_peer_certificate: No certificate was presented by the server" << std::endl;
            exit(1);
        }
        if (X509_check_host(cert, expected_hostname.data(), expected_hostname.size(), 0, nullptr) != 1)
        {
            std::cerr << "X509_check_host: Certificate verification error: Hostname mismatch" << std::endl;
            exit(1);
        }
    }
}