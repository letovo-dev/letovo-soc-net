#include "chat.h"
#include "../basic/url_parser.h"

namespace chat {

    bool is_chattable(const std::string& username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        cp::SafeCon con{pool_ptr};
        std::vector<std::string> params = {username};
        pqxx::result result = con->execute_params(
            "SELECT \"chattable\" FROM \"user\" WHERE \"username\"=($1);", params);
        if (result.empty()) {
            return false;
        }
        return result[0]["chattable"].as<bool>();
    }

    pqxx::result get_chattable_users(const std::string& current_user, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        cp::SafeCon con{pool_ptr};
        std::vector<std::string> params = {current_user};
        pqxx::result result = con->execute_params(
            "SELECT u.username, u.display_name, u.avatar_pic, "
            "dm.message_text AS last_message, dm.sent_at AS last_message_time "
            "FROM \"user\" u "
            "LEFT JOIN LATERAL ("
            "  SELECT message_text, sent_at FROM direct_message "
            "  WHERE (sender = u.username AND receiver = $1) "
            "     OR (sender = $1 AND receiver = u.username) "
            "  ORDER BY sent_at DESC LIMIT 1"
            ") dm ON true "
            "WHERE u.chattable = true "
            "ORDER BY dm.sent_at DESC NULLS LAST, u.username;", params);
        return result;
    }

    pqxx::result get_messages(const std::string& user1, const std::string& user2, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        cp::SafeCon con{pool_ptr};
        std::vector<std::string> params = {user1, user2};
        pqxx::result result = con->execute_params(
            "SELECT dm.message_id, dm.sender, dm.receiver, dm.message_text, dm.sent_at, "
            "COALESCE(string_agg(ma.link, ',') FILTER (WHERE ma.link IS NOT NULL), '') AS attachments "
            "FROM direct_message dm "
            "LEFT JOIN message_attachments ma ON ma.message_id = dm.message_id "
            "WHERE (dm.sender = $1 AND dm.receiver = $2) OR (dm.sender = $2 AND dm.receiver = $1) "
            "GROUP BY dm.message_id "
            "ORDER BY dm.sent_at ASC;", params);
        return result;
    }

    int send_message(const std::string& sender, const std::string& receiver, const std::string& text,
                     const std::vector<std::string>& attachments, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());

        std::vector<std::string> params = {sender, receiver, text};
        pqxx::result result = con->execute_params(
            "INSERT INTO direct_message (sender, receiver, message_text) "
            "VALUES ($1, $2, $3) RETURNING message_id;", params, true);

        int message_id = result[0]["message_id"].as<int>();

        for (const auto& link : attachments) {
            std::vector<std::string> att_params = {std::to_string(message_id), link};
            con->execute_params(
                "INSERT INTO message_attachments (message_id, link) VALUES ($1, $2);",
                att_params, true);
        }

        pool_ptr->returnConnection(std::move(con));
        return message_id;
    }

}

namespace chat::server {

    void get_chats(std::unique_ptr<restinio::router::express_router_t<>>& router,
                   std::shared_ptr<cp::ConnectionsManager> pool_ptr,
                   std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/chats/", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /chats/";});
            std::string token;
            try {
                token = req->header().get_field("Bearer");
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            std::string username = auth::get_username(token, pool_ptr);
            if (username.empty()) {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            pqxx::result result = chat::get_chattable_users(username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_chat(std::unique_ptr<restinio::router::express_router_t<>>& router,
                  std::shared_ptr<cp::ConnectionsManager> pool_ptr,
                  std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/chat/:username([a-zA-Z0-9\-_]+))", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /chat/:username";});
            std::string token;
            try {
                token = req->header().get_field("Bearer");
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            std::string current_user = auth::get_username(token, pool_ptr);
            if (current_user.empty()) {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            std::string target_user = url::get_last_url_arg(req->header().path());
            if (target_user.empty()) {
                return req->create_response(restinio::status_bad_request()).done();
            }
            pqxx::result result = chat::get_messages(current_user, target_user, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void new_message(std::unique_ptr<restinio::router::express_router_t<>>& router,
                     std::shared_ptr<cp::ConnectionsManager> pool_ptr,
                     std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_post("/new_message", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /new_message";});
            std::string token;
            try {
                token = req->header().get_field("Bearer");
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            std::string sender = auth::get_username(token, pool_ptr);
            if (sender.empty()) {
                return req->create_response(restinio::status_unauthorized()).done();
            }

            rapidjson::Document body;
            body.Parse(req->body().c_str());

            if (!body.HasMember("receiver") || !body.HasMember("text")) {
                return req->create_response(restinio::status_bad_request()).done();
            }

            std::string receiver = body["receiver"].GetString();
            std::string text = body["text"].GetString();

            if (!chat::is_chattable(receiver, pool_ptr)) {
                return req->create_response(restinio::status_forbidden())
                    .set_body(R"({"error": "receiver is not chattable"})")
                    .append_header("Content-Type", "application/json; charset=utf-8")
                    .done();
            }

            std::vector<std::string> attachments;
            if (body.HasMember("attachments") && body["attachments"].IsArray()) {
                for (auto& v : body["attachments"].GetArray()) {
                    if (v.IsString()) {
                        attachments.emplace_back(v.GetString());
                    }
                }
            }

            try {
                int message_id = chat::send_message(sender, receiver, text, attachments, pool_ptr);
                std::string response = fmt::format(
                    R"({{"message_id": {}, "sender": "{}", "receiver": "{}", "status": "sent"}})",
                    message_id, sender, receiver);
                return req->create_response()
                    .set_body(response)
                    .append_header("Content-Type", "application/json; charset=utf-8")
                    .done();
            } catch (const std::exception& e) {
                logger_ptr->error([e]{return fmt::format("error sending message: {}", e.what());});
                return req->create_response(restinio::status_internal_server_error()).done();
            }
        });
    }

}
