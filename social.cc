#include "social.h"

namespace social {

    pqxx::result get_authors_list(std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        pqxx::result result = con->execute("SELECT \"username\", \"avatar_pic\" from \"user\" where \"author\"=true;");
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_news(std::string start, int size, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {username, start, std::to_string(size)};
        pqxx::result result = con->execute_params("SELECT p.*, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id WHERE p.parent_id is null ORDER BY p.post_id DESC offset ($2) LIMIT ($3);", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_comments(std::string post_id, std::string start, int size, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {username, post_id, start, std::to_string(size)};
        pqxx::result result = con->execute_params("SELECT p.*, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id where p.parent_id is not null and p.parent_id = ($2) ORDER BY p.post_id DESC offset ($3) LIMIT ($4);", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_post_media(std::string post_id, bool pics, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {post_id};
        pqxx::result result;
        if(pics) {
            result = con->execute_params("SELECT * FROM \"post_media\" WHERE \"post_media\".post_id=($1) AND \"post_media\".is_pic=true;", params);
        } else {
            result = con->execute_params("SELECT * FROM \"post_media\" WHERE \"post_media\".post_id=($1) AND \"post_media\".is_pic=false;", params);
        }
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    void add_like(int like, std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {std::to_string(like), post_id, username};
        con->execute_params("INSERT INTO \"user_likes\" (\"value\", \"post_id\", \"username\") VALUES ($1, $2, $3) ON CONFLICT (post_id, username) DO update SET \"value\" = EXCLUDED.\"value\";", params, true);
        con->execute_params("UPDATE \"posts\" SET \"posts\".likes = \"posts\".likes + ($1) WHERE \"posts\".post_id=($2);", params, true);
        pool_ptr->returnConnection(std::move(con));
    }
}

namespace social::server {
    void get_authors_list(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/authors", [pool_ptr, logger_ptr](auto req, auto) {
            pqxx::result result = social::get_authors_list(pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_news(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/news", [pool_ptr, logger_ptr](auto req, auto) {
            std::string start = req->header().get_field("start");
            int size = std::min(10, std::stoi(req->header().get_field("size")));
            std::string token;
            try {
                token = req -> header().get_field("Bearer");
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            std::string username = auth::get_username(token, pool_ptr);
            if(username == "") {
                return req->create_response(restinio::status_unauthorized()).done();
            }

            pqxx::result result = social::get_news(start, size, username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_comments(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/comments", [pool_ptr, logger_ptr](auto req, auto) {
            std::string post_id = req->header().get_field("postid");
            std::string start = req->header().get_field("start");
            int size = std::min(10, std::stoi(req->header().get_field("size")));
            std::string token;
            try {
                token = req -> header().get_field("Bearer");
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            std::string username = auth::get_username(token, pool_ptr);
            if(username == "") {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            pqxx::result result = social::get_comments(post_id, start, size, username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_post_media(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/media", [pool_ptr, logger_ptr](auto req, auto) {
            std::string post_id = req->header().get_field("postid");
            bool pics = req->header().get_field("pics") == "true";
            pqxx::result result = social::get_post_media(post_id, pics, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void add_like(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_post("/social/like", [pool_ptr, logger_ptr](auto req, auto) {
            std::string post_id = req->header().get_field("postid");
            int like = std::stoi(req->header().get_field("like"));
            std::string token;
            try {
                token = req -> header().get_field("Bearer");
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_unauthorized()).done();
            }
            std::string username = auth::get_username(token, pool_ptr);
            if(username == "") {
                return req->create_response(restinio::status_unauthorized()).done();
            }

            social::add_like(like, post_id, username, pool_ptr);
            return req->create_response().done();
        });
    }
}