#include "social.h"

namespace social {

    pqxx::result get_authors_list(std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        pqxx::result result = con->execute("SELECT \"username\", \"avatar_pic\", \"display_name\" from \"user\" where \"author\"=true;");
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_news(std::string start, int size, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {username, start, std::to_string(size)};
        pqxx::result result = con->execute_params("SELECT DISTINCT p.*, u.avatar_pic, u.display_name, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked, case when s.username is not null then true else false end as saved from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id AND l.username = $1 left join \"user_saved\" s on p.post_id = s.post_id and s.username = $1 left join \"user\" u on p.author = u.username WHERE p.parent_id is null and p.post_path is null ORDER BY p.date DESC offset ($2) LIMIT ($3);", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_comments(std::string post_id, std::string start, int size, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        // std::vector<std::string> params = {username, post_id, start, std::to_string(size)};
        std::vector<std::string> params = {username, start, std::to_string(size), post_id};
        // pqxx::result result = con->execute_params("SELECT p.*, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id where p.parent_id is not null and p.parent_id = ($2) ORDER BY p.post_id DESC offset ($3) LIMIT ($4);", params);
        pqxx::result result = con->execute_params("SELECT DISTINCT p.*, u.avatar_pic, u.display_name, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked, case when s.username is not null then true else false end as saved from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id AND l.username = $1 left join \"user_saved\" s on p.post_id = s.post_id and s.username = $1 left join \"user\" u on p.author = u.username WHERE p.parent_id = ($4) and p.post_path is null ORDER BY p.date DESC offset ($2) LIMIT ($3);", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_post_media(std::string post_id, bool pics, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {post_id};
        pqxx::result result;
        result = con->execute_params("SELECT * FROM \"post_media\" WHERE \"post_media\".post_id=($1) and \"post_media\".is_secret=false;", params);
        
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    // FIXME: bug select always returns empty result
    void add_like(int like, std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {std::to_string(like), post_id, username};
        auto check = con->execute_params("SELECT * FROM \"user_likes\" ul WHERE ul.post_id=($2) AND ul.username=($3) and ul.value!=($1);", params);
        params = {post_id};
        if(check.size() > 0) {
            if(like == 1) {
                con->execute_params("UPDATE \"posts\" SET dislikes = dislikes - 1 WHERE post_id=($2);", params, true);
            } else if(like == -1) {
                con->execute_params("UPDATE \"posts\" SET likes = likes - 1 WHERE post_id=($2);", params, true);
            }
        }
        params = {post_id, username};
        con->execute_params("DELETE FROM \"user_likes\" WHERE post_id=($1) AND username=($2);", params, true);
        params = {std::to_string(like), post_id, username};
        con->execute_params("INSERT INTO \"user_likes\" (\"value\", \"post_id\", \"username\") VALUES ($1, $2, $3) ON CONFLICT (post_id, username) DO NOTHING;", params, true);
        params = {post_id};
        if(like == 1) {
            con->execute_params("UPDATE \"posts\" SET likes = likes + 1 WHERE post_id=($1);", params, true);
        } else if(like == -1) {
            con->execute_params("UPDATE \"posts\" SET dislikes = dislikes + 1 WHERE post_id=($1);", params, true);
        }
        pool_ptr->returnConnection(std::move(con));
    }

    void delete_like(int like, std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {post_id, username};
        con->execute_params("DELETE FROM \"user_likes\" WHERE post_id=($1) AND username=($2);", params, true);
        params = {post_id};
        if(like == 1) {
            con->execute_params("UPDATE \"posts\" SET likes = likes - 1 WHERE post_id=($1);", params, true);
        } else if(like == -1) {
            con->execute_params("UPDATE \"posts\" SET dislikes = dislikes - 1 WHERE post_id=($1);", params, true);
        }
        pool_ptr->returnConnection(std::move(con));
    }


    std::string escape_newlines(const std::string& input) {
        std::string output;
        output.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '\r') {
                if (i + 1 < input.size() && input[i + 1] == '\n') {
                    ++i;
                }
                output += "\\n";
            } else if (input[i] == '\n') {
                output += "\\n";
            } else {
                output += input[i];
            }
        }

        return output;
    }

    int add_comment(std::string comment, std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {escape_newlines(comment), post_id, username};
        pqxx::result result = con->execute_params("INSERT INTO \"posts\" (\"text\", \"parent_id\", \"author\") VALUES ($1, $2, $3) returning \"post_id\";", params, true);
        pool_ptr->returnConnection(std::move(con));
        return result[0]["post_id"].as<int>();
    }

    pqxx::result get_all_titles(std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        pqxx::result result = con->execute("SELECT post_id, title FROM \"posts\" where \"posts\".post_path is null and \"posts\".parent_id is null;");
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_all_posts(std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {username};
        pqxx::result result = con->execute_params("SELECT DISTINCT p.*, u.avatar_pic, u.display_name, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked, case when s.username is not null then true else false end as saved from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id AND l.username = $1 left join \"user_saved\" s on p.post_id = s.post_id and s.username = $1 left join \"user\" u on p.author = u.username WHERE p.parent_id is null and p.post_path is not null ORDER BY p.date;", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    pqxx::result get_saved_posts(std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {username};
        pqxx::result result = con->execute_params("SELECT p.*, u.avatar_pic, u.display_name, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked, case when s.username is not null then true else false end as saved from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id AND l.username = ($1) left join \"user_saved\" s on p.post_id = s.post_id and s.username = ($1) left join \"user\" u on p.author = u.username WHERE s.username=($1);", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }

    void save_post(std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {post_id, username};
        con->execute_params("INSERT INTO \"user_saved\" (\"post_id\", \"username\") VALUES ($1, $2);", params, true);
        params = {post_id};
        con->execute_params("UPDATE \"posts\" SET saved_count = saved_count + 1 WHERE post_id=($1);", params, true);
        pool_ptr->returnConnection(std::move(con));
    }

    void delete_saved_post(std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {post_id, username};
        con->execute_params("DELETE FROM \"user_saved\" WHERE post_id=($1) AND username=($2);", params, true);
        params = {post_id};
        con->execute_params("UPDATE \"posts\" SET saved_count = saved_count - 1 WHERE post_id=($1);", params, true);
        pool_ptr->returnConnection(std::move(con));
    }

    pqxx::result get_post(std::string post_id, std::string username, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());
        std::vector<std::string> params = {username, post_id};
        pqxx::result result = con->execute_params("SELECT DISTINCT p.*, u.avatar_pic, u.display_name, case when l.username = ($1) and l.value = 1 then true else false end as is_liked, case when l.username = ($1) and l.value = -1 then true else false end as is_disliked, case when s.username is not null then true else false end as saved from \"posts\" p left join \"user_likes\" l on l.post_id = p.post_id AND l.username = ($1) left join \"user_saved\" s on p.post_id = s.post_id and s.username = ($1) left join \"user\" u on p.author = u.username WHERE p.post_id = ($2)", params);
        pool_ptr->returnConnection(std::move(con));
        return result;
    }


    pqxx::result get_post_categories(std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());

        pqxx::result result = con->execute("select pc.category_id as category, pc.category_name from \"post_category\" pc;");

        pool_ptr->returnConnection(std::move(con));

        if (result.empty()) {
            return {};
        }
        return result;
    }

    pqxx::result get_post_by_category(std::string category, std::shared_ptr<cp::ConnectionsManager> pool_ptr) {
        auto con = std::move(pool_ptr->getConnection());

        std::vector<std::string> params = {category};

        pqxx::result result = con->execute_params("select p.*, pc.category_name from \"posts\" p left join \"post_category\" pc on p.category = pc.category_id where p.category = ($1) AND p.post_path is not null;", params);

        pool_ptr->returnConnection(std::move(con));

        if (result.empty()) {
            return {};
        }
        return result;
    }

}

namespace social::server {
    void get_authors_list(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/authors", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/authors";});
            pqxx::result result = social::get_authors_list(pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_news(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/news:search(.*))", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/news";});
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
                .set_body(cp::serialize_with_segment_day(result, pool_ptr))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_comments(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/comments:search(.*))", [pool_ptr, logger_ptr](auto req, auto) {            
            logger_ptr->trace([]{return "called /social/comments";});
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
            logger_ptr->trace([]{return "called /social/media/pics/:post_id";});
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
            logger_ptr->trace([]{return "called /social/media/pics/:post_id";});
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
            logger_ptr->trace([]{return "called /social/new/:post_id";});
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
            pqxx::result result = social::get_post(post_id, username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_all_posts(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/posts", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/posts";});
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
            pqxx::result result = social::get_all_posts(username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_all_titles(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/titles", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/titles";});
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

    void get_saved_posts(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/saved", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/saved";});
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
            pqxx::result result = social::get_saved_posts(username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }
    void save_post(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_post("/social/save", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/save";});
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
            social::save_post(new_body["post_id"].GetString(), username, pool_ptr);
            return req->create_response().done();
        });
    }
    void delete_saved_post(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_delete("/social/save", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/save";});
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
            social::delete_saved_post(new_body["post_id"].GetString(), username, pool_ptr);
            return req->create_response().done();
        });
    }

    void search_by_title(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/search:search(.*))", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/search";});
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
            pqxx::result result = social::get_post("111", username, pool_ptr);
            return req->create_response()
                .set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void add_like(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_post("/social/like", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/like";});
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

    void add_dislike(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_post("/social/dislike", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/dislike";});
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
                social::delete_like(1, new_body["post_id"].GetString(), username, pool_ptr);
                social::add_like(-1, new_body["post_id"].GetString(), username, pool_ptr);
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_internal_server_error()).done();
            }
            return req->create_response().done();
        });
    }

    void delete_like(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_delete("/social/like", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/like";});
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
                social::delete_like(1, new_body["post_id"].GetString(), username, pool_ptr);
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_internal_server_error()).done();
            }
            return req->create_response().done();
        });
    }

    void delete_dislike(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_delete("/social/dislike", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/dislike";});
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
                social::delete_like(-1, new_body["post_id"].GetString(), username, pool_ptr);
            } catch (const std::exception& e) {
                return req->create_response(restinio::status_internal_server_error()).done();
            }
            return req->create_response().done();
        });
    }

    void add_comment(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_post("/social/comments", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/comments";});
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
            if(new_body.HasMember("author") && authors::check_if_avaluable_author(username, new_body["author"].GetString(), pool_ptr)) {
                username = new_body["author"].GetString();
            }
            try {
                std::string comment = new_body["comment"].GetString();
                assist::fix_new_lines(comment);
                int post_id = social::add_comment(comment, new_body["post_id"].GetString(), username, pool_ptr);
                pqxx::result result = social::get_post(std::to_string(post_id), username, pool_ptr);
                return req->create_response()
                    .set_body(cp::serialize(result))
                    .append_header("Content-Type", "application/json; charset=utf-8")
                    .done();
            } catch(const std::exception& e) {
                logger_ptr->error([e] { return fmt::format("Error: {}", e.what()); });
                return req->create_response(restinio::status_internal_server_error()).done();
            }
        });
    }


    void get_post_categories(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get("/social/categories", [pool_ptr, logger_ptr](auto req, auto) {
            logger_ptr->trace([]{return "called /social/categories";});
            pqxx::result result = social::get_post_categories(pool_ptr);
            if (result.empty()) {
                return req->create_response(restinio::status_bad_gateway()).done();
            }
            return req->create_response().set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }

    void get_post_by_category(std::unique_ptr<restinio::router::express_router_t<>>& router, std::shared_ptr<cp::ConnectionsManager> pool_ptr, std::shared_ptr<restinio::shared_ostream_logger_t> logger_ptr) {
        router.get()->http_get(R"(/social/bycat/:category([0-9\-]+))", [pool_ptr, logger_ptr](auto req, auto params) {
            logger_ptr->trace([]{return "called /social/bycat/:category";});
            auto qrl = req->header().path();

            std::string category = url::get_last_url_arg(qrl);

            logger_ptr->info([category] { return fmt::format("get post by category request for {}", category); });

            if (category == "category" || category.empty()) {
                return req->create_response(restinio::status_bad_request()).done();
            }
            pqxx::result result;
            try {
                result = social::get_post_by_category(category, pool_ptr);
            } catch (const std::exception& e) {
                logger_ptr->error([e] { return fmt::format("Error: {}", e.what()); });
                return req->create_response(restinio::status_internal_server_error()).done();
            }

            if (result.empty()) {
                return req->create_response(restinio::status_bad_gateway()).done();
            }
            return req->create_response().set_body(cp::serialize(result))
                .append_header("Content-Type", "application/json; charset=utf-8")
                .done();
        });
    }
}