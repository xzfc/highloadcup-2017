#include "all.hpp"
#include "json-serialize.hpp"
#include "bench.hpp"

#include <boost/lexical_cast.hpp>

#include "simple-web-server/server_http.hpp"

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef std::shared_ptr<HttpServer::Response> ResponsePtr;
typedef std::shared_ptr<HttpServer::Request> RequestPtr;

static uint32_t atou32(const std::string &str) {
	return atoll(str.c_str());
}

static void do_resp(
		HttpServer::Response &response,
		int code,
		const std::string &data)
{
	switch (code) {
	case 200: response << "HTTP/1.1 200 OK\r\n";                    break;
	case 400: response << "HTTP/1.1 400 Bad Request\r\n";           break;
	case 404: response << "HTTP/1.1 404 Not Found\r\n";             break;
	case 500: response << "HTTP/1.1 500 Internal Server Error\r\n"; break;
	}
	response << "Content-Length: " << data.length() << "\r\n\r\n" << data;
}

void start_server(All &all, uint16_t port) {
	HttpServer server;
	server.config.port = port;

	server.resource["^/users/([0-9]+)$"]["GET"] =
		[&all](ResponsePtr response, RequestPtr request) {
			uint32_t id = atou32(request->path_match[1]);
			if (User *user = all.get_user(id))
				do_resp(*response, 200, json_serialize(id, *user));
			else
				do_resp(*response, 404, "Not found");
		};

	server.resource["^/locations/([0-9]+)$"]["GET"] =
		[&all](ResponsePtr response, RequestPtr request) {
			uint32_t id = atou32(request->path_match[1]);
			if (Location *location = all.get_location(id))
				do_resp(*response, 200, json_serialize(id, *location));
			else
				do_resp(*response, 404, "Not found");
		};

	server.resource["^/visits/([0-9]+)$"]["GET"] =
		[&all](ResponsePtr response, RequestPtr request) {
			uint32_t id = atou32(request->path_match[1]);
			if (Visit *visit = all.get_visit(id))
				do_resp(*response, 200, json_serialize(id, *visit));
			else
				do_resp(*response, 404, "Not found");
		};

	bench b0, b1, b2, b3, b4;
	server.resource["^/users/([0-9]+)/visits$"]["GET"] =
		[&](ResponsePtr response, RequestPtr request) {
			auto bt = bench::now();
			uint32_t id = atou32(request->path_match[1]);
			b0.ok(bt);

			bt = bench::now();
			User *user = all.get_user(id);
			if (user == nullptr) {
				do_resp(*response, 404, "Not found");
				return;
			}
			b1.ok(bt);

			boost::optional<time_t> from_date;
			boost::optional<time_t> to_date;
			boost::optional<std::string> country;
			boost::optional<uint32_t> to_distance;

			bt = bench::now();
			auto query = request->parse_query_string();
			b2.ok(bt);

			bt = bench::now();
			try {
				auto it = query.find("fromDate");
				if (it != query.end())
					from_date = boost::lexical_cast<time_t>(it->second);

				it = query.find("toDate");
				if (it != query.end())
					to_date = boost::lexical_cast<time_t>(it->second);

				it = query.find("country");
				if (it != query.end())
					country = it->second;

				it = query.find("toDistance");
				if (it != query.end())
					to_distance = boost::lexical_cast<time_t>(it->second);
			} catch (boost::bad_lexical_cast) {
				do_resp(*response, 400, "Bad param");
				return;
			}
			b3.ok(bt);

			static thread_local std::vector<VisitData> out;
			bt = bench::now();
			bool ok = all.get_visits(
					out, id, from_date, to_date, country, to_distance);
			b4.ok(bt);
			if (ok)
				do_resp(*response, 200, json_serialize(out));
			else
				do_resp(*response, 404, "Not found");
		};

	server.resource["^/locations/([0-9]+)/avg$"]["GET"] = 
		[&all](ResponsePtr response, RequestPtr request) {
			uint32_t id = atou32(request->path_match[1]);

		};

	server.resource["^/info$"]["GET"] = 
		[&](ResponsePtr response, RequestPtr request) {
			auto resp = b0.str() + b1.str() + b2.str() + b3.str() + b4.str();
			do_resp(*response, 200, resp);
		};


	server.default_resource["GET"] =
		[](ResponsePtr response, RequestPtr) {
			do_resp(*response, 404, "");
		};

	std::cout << "Started\n";
	server.start();
}
