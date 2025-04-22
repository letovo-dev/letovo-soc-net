#pragma once
#include <restinio/all.hpp>
#include <pqxx/pqxx>
#include <vector>
#include <fmt/format.h>
#include <rapidjson/document.h>
#include ".././basic/pqxx_cp.h"
#include <unordered_map>
#include <filesystem>
#include <any>
#include "../basic/assist_funcs.h"
#include "../basic/url_parser.h"
#include "../basic/auth.h"
#include "../basic/media.h"
#include "../basic/config.h"

namespace social {
    pqxx::result get_authors_list(std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    pqxx::result get_news(std::string start, int size, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    pqxx::result get_comments(std::string post_id, std::string start, int size, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr);
        
    pqxx::result get_post_media(std::string post_id, bool pics, std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    pqxx::result get_all_titles(std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    pqxx::result get_post(std::string post_id, std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    void add_like(int like, std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr);

    void add_comment(std::string comment, std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr);
}

namespace social::server {
    void get_authors_list(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void get_news(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void get_comments(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void get_post_media(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void get_post_pics(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void get_post(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void get_all_titles(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void search_by_title(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void add_like(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);

    void add_comment(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr);
}