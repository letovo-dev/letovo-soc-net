#pragma once
#include <restinio/all.hpp>
#include <pqxx/pqxx>
#include <vector>
#include <string>
#include <fmt/format.h>
#include <rapidjson/document.h>
#include "../basic/pqxx_cp.h"
#include "../basic/auth.h"

namespace chat {
    bool is_chattable(const std::string& username, std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    pqxx::result get_chattable_users(const std::string& current_user, std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    pqxx::result get_messages(const std::string& user1, const std::string& user2, std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    int send_message(const std::string& sender, const std::string& receiver, const std::string& text, const std::vector<std::string>& attachments, std::shared_ptr<cp::ConnectionsManager> pool_ptr);
}

namespace chat::server {
    void get_chats(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void get_chat(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void new_message(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);
}
