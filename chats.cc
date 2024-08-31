#include <iostream>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <fstream>
#include <chrono>
#include <string>
#include <ctime>
#include <pqxx/pqxx>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

httplib::Server svr;

void handler() {
    svr.Get("/hi", [](const httplib::Request &req, httplib::Response &res) {
        
        res.set_content("placeholder", "placeholder 2 ");
    });
}


int main() {
    handler();
    svr.listen("0.0.0.0", 8080);
}