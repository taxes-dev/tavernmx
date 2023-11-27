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
        char buffer[1024];
        int len = BIO_read(bio, buffer, sizeof(buffer));
        if (len < 0) {
            tavernmx::ssl::print_errors_and_exit("error in BIO_read");
        } else if (len > 0) {
            return {buffer, static_cast<size_t>(len)};
        } else if (BIO_should_retry(bio)) {
            return receive_some_data(bio);
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

        BIO_write(bio, request.data(), static_cast<int>(request.size()));
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
}