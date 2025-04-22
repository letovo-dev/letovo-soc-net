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
        pqxx::result result = con->execute_params("SELECT DISTINCT p.*, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked, case when s.username is not null then true else false end as saved from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id AND l.username = $1 left join \"user_saved\" s on p.post_id = s.post_id and s.username = $1 WHERE p.parent_id is null ORDER BY p.post_id DESC offset ($2) LIMIT ($3);", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_comments(std::string post_id, std::string start, int size, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        // std::vector<std::string> params = {username, post_id, start, std::to_string(size)};
        std::vector<std::string> params = {post_id, start, std::to_string(size)};
        // pqxx::result result = con->execute_params("SELECT p.*, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id where p.parent_id is not null and p.parent_id = ($2) ORDER BY p.post_id DESC offset ($3) LIMIT ($4);", params);
        pqxx::result result = con->execute_params("SELECT * from \"comments\" where \"post_id\" = ($1) ORDER BY \"post_id\" DESC offset ($2) LIMIT ($3);", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_post_media(std::string post_id, bool pics, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {post_id};
        pqxx::result result;
        result = con->execute_params("SELECT * FROM \"post_media\" WHERE \"post_media\".post_id=($1);", params);
        // if(pics) {
        //     result = con->execute_params("SELECT * FROM \"post_media\" WHERE \"post_media\".post_id=($1) AND \"post_media\".is_pic=true;", params);
        // } else {
        //     result = con->execute_params("SELECT * FROM \"post_media\" WHERE \"post_media\".post_id=($1) AND \"post_media\".is_pic=false;", params);
        // }
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    void add_like(int like, std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {std::to_string(like), post_id, username};
        con->execute_params("INSERT INTO \"user_likes\" (\"value\", \"post_id\", \"username\") VALUES ($1, $2, $3) ON CONFLICT (post_id, username) DO NOTHING;", params, true);
        params = {std::to_string(like), post_id};
        con->execute_params("UPDATE \"posts\" SET likes = likes + ($1) WHERE post_id=($2);", params, true);
        pool_ptr->returnConnection(std::move(con));
    }

    void add_comment(std::string comment, std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {comment, post_id, username};
        con->execute_params("INSERT INTO \"comments\" (\"comment\", \"post_id\", \"username\") VALUES ($1, $2, $3);", params, true);
        pool_ptr->returnConnection(std::move(con));
    }

    pqxx::result get_all_titles(std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        pqxx::result result = con->execute("SELECT post_id, title FROM \"posts\";");
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_post(std::string post_id, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {post_id};
        pqxx::result result = con->execute_params("SELECT p.*, case when s.username is not null then true else false end as saved  FROM \"posts\" p left join \"user_saved\" s on p.post_id = s.post_id and s.username = 'scv-7' WHERE p.post_id=($1);", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
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
        router.get()->http_get(R"(/social/news:search(.*))", [pool_ptr, logger_ptr](auto req, auto) {
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
            const auto qp = restinio::parse_query( req->header().query() );
            
            if(!qp.has("start") || !qp.has("size")) {
                return req->create_response(restinio::status_bad_request()).done();
            }
            pqxx::result result = social::get_news((std::string)qp["start"], std::stoi((std::string)qp["size"]), username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_comments(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/comments:search(.*))", [pool_ptr, logger_ptr](auto req, auto) {            
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
            const auto qp = restinio::parse_query( req->header().query() );
            if(!qp.has("post_id") || !qp.has("start") || !qp.has("size")) {
                return req->create_response(restinio::status_bad_request()).done();
            }
            pqxx::result result = social::get_comments((std::string)qp["post_id"], (std::string)qp["start"], std::stoi((std::string)qp["size"]), username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_post_pics(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/media/pics/:post_id(.*))", [pool_ptr, logger_ptr](auto req, auto) {
            std::string post_id = url::get_last_url_arg(req->header().path());
            bool pics = true; //req->header().get_field("pics") == "true";
            pqxx::result result = social::get_post_media(post_id, pics, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_post_media(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/media/pics/:post_id(.*))", [pool_ptr, logger_ptr](auto req, auto) {
            std::string post_id = url::get_last_url_arg(req->header().path());
            bool pics = false; //req->header().get_field("pics") == "true";
            pqxx::result result = social::get_post_media(post_id, pics, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_post(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/new/:post_id(.*))", [pool_ptr, logger_ptr](auto req, auto) {
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
            logger_ptr->info([username] { return fmt::format("user {} get post request", username); });
            std::string post_id = url::get_last_url_arg(req->header().path());
            logger_ptr->info([post_id] { return fmt::format("get post request for {}", post_id); });
            pqxx::result result = social::get_post(post_id, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_all_titles(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/titles", [pool_ptr, logger_ptr](auto req, auto) {
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
            pqxx::result result = social::get_all_titles(pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void search_by_title(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/search:search(.*))", [pool_ptr, logger_ptr](auto req, auto) {
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
            const auto qp = restinio::parse_query( req->header().query() );
            if(!qp.has("title")) {
                return req->create_response(restinio::status_bad_request()).done();
            }
            pqxx::result result = social::get_post("81", pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void add_like(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_post("/social/like", [pool_ptr, logger_ptr](auto req, auto) {
            rapidjson::Document new_body;
            new_body.Parse(req->body().c_str());
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
            if(!new_body.HasMember("post_id")) {
                return req->create_response(restinio::status_bad_request()).done();
            }
            try {
                social::add_like(1, new_body["post_id"].GetString(), username, pool_ptr);
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_internal_server_error()).done();
            }
            return req->create_response().done();
        });
    }

    void add_comment(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_post("/social/comments", [pool_ptr, logger_ptr](auto req, auto) {
            rapidjson::Document new_body;
            new_body.Parse(req->body().c_str());
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
            if(!new_body.HasMember("post_id") || !new_body.HasMember("comment")) {
                return req->create_response(restinio::status_bad_request()).done();
            }
            try {
                social::add_comment(new_body["comment"].GetString(), new_body["post_id"].GetString(), username, pool_ptr);
            } catch(const std::exception& e) {
                logger_ptr->error([e] { return fmt::format("Error: {}", e.what()); });
                return req->create_response(restinio::status_internal_server_error()).done();
            }
            return req->create_response().done();
        });
    }
}