/**
 * @file mcp_sse_client.cpp
 * @brief Implementation of the MCP SSE client
 * 
 * This file implements the client-side functionality for the Model Context Protocol using SSE.
 * Follows the 2024-11-05 basic protocol specification.
 */

#include "mcp_sse_client.h"
#include "base64.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

struct parsed_url_components {
    std::string scheme;
    std::string host;
    int port = -1;
    std::string path;
};

std::string trim_copy(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

parsed_url_components parse_url_components(const std::string& url) {
    parsed_url_components result;
    std::string trimmed = trim_copy(url);

    if (trimmed.empty()) {
        return result;
    }

    size_t scheme_pos = trimmed.find("://");
    size_t authority_start = 0;

    if (scheme_pos != std::string::npos) {
        result.scheme = trimmed.substr(0, scheme_pos);
        std::transform(result.scheme.begin(), result.scheme.end(), result.scheme.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        authority_start = scheme_pos + 3;
    }

    size_t path_pos = trimmed.find('/', authority_start);
    const std::string authority =
        path_pos == std::string::npos ? trimmed.substr(authority_start)
                                      : trimmed.substr(authority_start, path_pos - authority_start);

    if (path_pos != std::string::npos) {
        result.path = trimmed.substr(path_pos);
    } else {
        result.path = "/";
    }

    auto sanitize_path = [](std::string path) {
        if (path.empty()) {
            return std::string("/");
        }
        size_t query_pos = path.find('?');
        if (query_pos != std::string::npos) {
            path.erase(query_pos);
        }
        size_t fragment_pos = path.find('#');
        if (fragment_pos != std::string::npos) {
            path.erase(fragment_pos);
        }
        if (path.empty()) {
            return std::string("/");
        }
        return path;
    };
    result.path = sanitize_path(result.path);

    if (!authority.empty()) {
        if (authority.front() == '[') {
            const size_t closing = authority.find(']');
            if (closing != std::string::npos) {
                result.host = authority.substr(1, closing - 1);
                if (closing + 1 < authority.size() && authority[closing + 1] == ':') {
                    const std::string port_str = authority.substr(closing + 2);
                    try {
                        result.port = std::stoi(port_str);
                    } catch (...) {
                    }
                }
            } else {
                result.host = authority;
            }
        } else {
            const size_t colon_pos = authority.rfind(':');
            if (colon_pos != std::string::npos) {
                result.host = authority.substr(0, colon_pos);
                const std::string port_str = authority.substr(colon_pos + 1);
                if (!port_str.empty()) {
                    try {
                        result.port = std::stoi(port_str);
                    } catch (...) {
                        result.port = -1;
                    }
                }
            } else {
                result.host = authority;
            }
        }
    }

    if (result.port <= 0) {
        if (result.scheme == "https") {
            result.port = 443;
        } else if (result.scheme == "http") {
            result.port = 80;
        }
    }

    return result;
}

std::string normalize_base_path(const std::string& path) {
    std::string value = trim_copy(path);
    if (value.empty()) {
        return "/";
    }
    if (value.front() != '/') {
        value.insert(value.begin(), '/');
    }
    while (value.size() > 1 && value.back() == '/') {
        value.pop_back();
    }
    return value.empty() ? std::string("/") : value;
}

bool is_absolute_url(const std::string& value) {
    const auto trimmed = trim_copy(value);
    const auto scheme_pos = trimmed.find("://");
    return scheme_pos != std::string::npos && scheme_pos > 0;
}

} // namespace

namespace mcp {

sse_client::sse_client(const std::string& host, int port, const std::string& sse_endpoint)
    : host_(host), port_(port), sse_endpoint_(sse_endpoint) {
    init_client(host, port);
}

sse_client::sse_client(const std::string& base_url, const std::string& sse_endpoint)
    : base_url_(base_url), sse_endpoint_(sse_endpoint) {
    init_client(base_url);
}

sse_client::~sse_client() {
    close_sse_connection();
}

void sse_client::init_client(const std::string& host, int port) {
    if (host.find("://") != std::string::npos) {
        configure_from_url(host);
        if (port > 0) {
            port_ = port;
            apply_timeouts();
        }
        return;
    }
    configure_from_components("", host, port, "/");
}

void sse_client::init_client(const std::string& base_url) {
    configure_from_url(base_url);
}

void sse_client::configure_from_components(const std::string& scheme,
                                           const std::string& host,
                                           int port,
                                           const std::string& base_path) {
    if (host.empty()) {
        throw std::invalid_argument("MCP SSE client host cannot be empty");
    }

    host_ = host;
    scheme_ = scheme;
    if (scheme_.empty()) {
        scheme_ = (port == 443) ? "https" : "http";
    }
    std::transform(scheme_.begin(), scheme_.end(), scheme_.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    use_ssl_ = (scheme_ == "https");

    if (port <= 0) {
        port_ = use_ssl_ ? 443 : 80;
    } else {
        port_ = port;
    }

    base_path_ = normalize_base_path(base_path);

    http_client_ = make_http_client();
    sse_client_ = make_http_client();

    if (http_client_) {
        http_client_->set_follow_location(true);
    }
    if (sse_client_) {
        sse_client_->set_follow_location(true);
    }

    apply_timeouts();
    apply_default_headers();
}

void sse_client::configure_from_url(const std::string& url) {
    parsed_url_components parsed = parse_url_components(url);
    std::string scheme = parsed.scheme.empty() ? "http" : parsed.scheme;
    if (parsed.host.empty()) {
        throw std::invalid_argument("MCP SSE client URL is missing host");
    }
    configure_from_components(scheme, parsed.host, parsed.port, parsed.path);
}

std::unique_ptr<httplib::Client> sse_client::make_http_client() const {
    auto build_scheme_host = [&](const std::string& scheme) {
        std::string host_component = host_;
        if (host_component.find(':') != std::string::npos && host_component.front() != '[') {
            host_component = "[" + host_component + "]";
        }
        std::string result = scheme + "://" + host_component;
        if (port_ > 0) {
            result += ":" + std::to_string(port_);
        }
        return result;
    };

    if (use_ssl_) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        auto client = std::make_unique<httplib::Client>(build_scheme_host(scheme_));
        client->enable_server_certificate_verification(true);
        return client;
#else
        throw std::runtime_error("HTTPS support requires CPPHTTPLIB_OPENSSL_SUPPORT");
#endif
    }

    if (!scheme_.empty() && scheme_ != "http") {
        return std::make_unique<httplib::Client>(build_scheme_host(scheme_));
    }

    return std::make_unique<httplib::Client>(host_.c_str(), port_);
}

void sse_client::apply_timeouts() {
    if (http_client_) {
        http_client_->set_connection_timeout(timeout_seconds_, 0);
        http_client_->set_read_timeout(timeout_seconds_, 0);
        http_client_->set_write_timeout(timeout_seconds_, 0);
    }

    if (sse_client_) {
        sse_client_->set_connection_timeout(timeout_seconds_ * 2, 0);
        sse_client_->set_read_timeout(0, 0);
        sse_client_->set_write_timeout(timeout_seconds_, 0);
    }
}

void sse_client::apply_default_headers() {
    httplib::Headers headers;
    for (const auto& [key, value] : default_headers_) {
        headers.emplace(key, value);
    }

    if (http_client_) {
        http_client_->set_default_headers(headers);
    }
    if (sse_client_) {
        sse_client_->set_default_headers(headers);
    }
}

std::string sse_client::resolve_endpoint_path(const std::string& raw) const {
    std::string trimmed = trim_copy(raw);
    if (trimmed.empty()) {
        return base_path_;
    }

    if (is_absolute_url(trimmed)) {
        parsed_url_components parsed = parse_url_components(trimmed);
        return parsed.path.empty() ? std::string("/") : parsed.path;
    }

    if (trimmed.front() == '/') {
        if (base_path_ != "/" && trimmed.compare(0, base_path_.size(), base_path_) != 0) {
            return base_path_ + trimmed;
        }
        return trimmed;
    }

    if (base_path_.empty() || base_path_ == "/") {
        return "/" + trimmed;
    }

    return base_path_ + "/" + trimmed;
}

bool sse_client::initialize(const std::string& client_name, const std::string& client_version) {
    LOG_INFO("Initializing MCP client...");
    
    request req = request::create("initialize", {
        {"protocolVersion", MCP_VERSION},
        {"capabilities", capabilities_},
        {"clientInfo", {
            {"name", client_name},
            {"version", client_version}
        }}
    });
    
    try {
        LOG_INFO("Opening SSE connection...");
        open_sse_connection();
        
        const auto timeout = std::chrono::milliseconds(5000);
        
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            bool success = endpoint_cv_.wait_for(lock, timeout, [this]() {
                if (!sse_running_) {
                    LOG_WARNING("SSE connection closed, stopping wait");
                    return true;
                }
                if (!msg_endpoint_.empty()) {
                    LOG_INFO("Message endpoint set, stopping wait");
                    return true;
                }
                return false;
            });
            
            if (!success) {
                LOG_WARNING("Condition variable wait timed out");
            }
            
            if (!sse_running_) {
                throw std::runtime_error("SSE connection closed, failed to get message endpoint");
            }
            
            if (msg_endpoint_.empty()) {
                throw std::runtime_error("Timeout waiting for SSE connection, failed to get message endpoint");
            }
            
            LOG_INFO("Successfully got message endpoint: ", msg_endpoint_);
        }

        json result = send_jsonrpc(req);
        
        server_capabilities_ = result["capabilities"];
        
        request notification = request::create_notification("initialized");
        send_jsonrpc(notification);
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Initialization failed: ", e.what());
        close_sse_connection();
        return false;
    }
}

bool sse_client::ping() {
    request req = request::create("ping", {});
    
    try {
        json result = send_jsonrpc(req);
        return result.empty();
    } catch (...) {
        return false;
    }
}

void sse_client::set_auth_token(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auth_token_ = token;
    if (auth_token_.empty()) {
        default_headers_.erase("Authorization");
    } else {
        const std::string bearer_prefix = "Bearer ";
        if (auth_token_.rfind(bearer_prefix, 0) == 0) {
            default_headers_["Authorization"] = auth_token_;
        } else {
            default_headers_["Authorization"] = bearer_prefix + auth_token_;
        }
    }
    apply_default_headers();
}

void sse_client::set_header(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (value.empty()) {
        default_headers_.erase(key);
    } else {
        default_headers_[key] = value;
    }
    apply_default_headers();
}

void sse_client::set_timeout(int timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    timeout_seconds_ = timeout_seconds;
    apply_timeouts();
}

void sse_client::set_capabilities(const json& capabilities) {
    std::lock_guard<std::mutex> lock(mutex_);
    capabilities_ = capabilities;
}

response sse_client::send_request(const std::string& method, const json& params) {
    request req = request::create(method, params);
    json result = send_jsonrpc(req);
    
    response res;
    res.jsonrpc = "2.0";
    res.id = req.id;
    res.result = result;
    
    return res;
}

void sse_client::send_notification(const std::string& method, const json& params) {
    request req = request::create_notification(method, params);
    send_jsonrpc(req);
}

json sse_client::get_server_capabilities() {
    return server_capabilities_;
}

json sse_client::call_tool(const std::string& tool_name, const json& arguments) {
    return send_request("tools/call", {
        {"name", tool_name},
        {"arguments", arguments}
    }).result;
}

std::vector<tool> sse_client::get_tools() {
    json response_json = send_request("tools/list", {}).result;
    std::vector<tool> tools;
    
    json tools_json;
    if (response_json.contains("tools") && response_json["tools"].is_array()) {
        tools_json = response_json["tools"];
    } else if (response_json.is_array()) {
        tools_json = response_json;
    } else {
        return tools;
    }
    
    for (const auto& tool_json : tools_json) {
        tool t;
        t.name = tool_json["name"];
        t.description = tool_json["description"];
        
        if (tool_json.contains("inputSchema")) {
            t.parameters_schema = tool_json["inputSchema"];
        }
        
        tools.push_back(t);
    }
    
    return tools;
}

json sse_client::get_capabilities() {
    return capabilities_;
}

json sse_client::list_resources(const std::string& cursor) {
    json params = json::object();
    if (!cursor.empty()) {
        params["cursor"] = cursor;
    }
    return send_request("resources/list", params).result;
}

json sse_client::read_resource(const std::string& resource_uri) {
    return send_request("resources/read", {
        {"uri", resource_uri}
    }).result;
}

json sse_client::subscribe_to_resource(const std::string& resource_uri) {
    return send_request("resources/subscribe", {
        {"uri", resource_uri}
    }).result;
}

json sse_client::list_resource_templates() {
    return send_request("resources/templates/list").result;
}

void sse_client::open_sse_connection() {
    sse_running_ = true;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        msg_endpoint_.clear();
        endpoint_cv_.notify_all();
    }
    
    const std::string resolved_sse_path = resolve_endpoint_path(sse_endpoint_);

    std::string connection_info;
    if (!base_url_.empty()) {
        connection_info = "Base URL: " + base_url_ + ", SSE Endpoint: " + sse_endpoint_ +
                          ", Resolved Path: " + resolved_sse_path;
    } else {
        connection_info = "Host: " + host_ + ", Port: " + std::to_string(port_) +
                          ", SSE Endpoint: " + sse_endpoint_ + ", Resolved Path: " + resolved_sse_path;
    }
    LOG_INFO("Attempting to establish SSE connection: ", connection_info);
    
    sse_thread_ = std::make_unique<std::thread>([this, resolved_sse_path]() {
        int retry_count = 0;
        const int max_retries = 5;
        const int retry_delay_base = 1000;
        
        while (sse_running_) {
            try {
                LOG_INFO("SSE thread: Attempting to connect to ", resolved_sse_path);
                
                std::string buffer;
                auto res = sse_client_->Get(resolved_sse_path.c_str(), 
                    [&,this](const char *data, size_t data_length) {
                        buffer.append(data, data_length);
                        
                        // Normalize CRLF to LF
                        size_t crlf_pos = buffer.find("\r\n");
                        while (crlf_pos != std::string::npos) {
                            buffer.replace(crlf_pos, 2, "\n");
                            crlf_pos = buffer.find("\r\n", crlf_pos + 1);
                        }
                        
                        // Process complete events in buffer
                        size_t start_pos = 0;
                        while ((start_pos = buffer.find("\n\n", start_pos)) != std::string::npos) {
                            size_t end_pos = start_pos + 2;
                            std::string event = buffer.substr(0, start_pos);
                            buffer.erase(0, end_pos);
                            start_pos = 0;
                            
                            if (!parse_sse_data(event.data(), event.size())) {
                                LOG_ERROR("SSE thread: Failed to parse event");
                            }
                        }
                        
                        return sse_running_.load();
                    });
                
                if (!res || res->status / 100 != 2) {
                    std::string error_msg = "SSE connection failed: ";
                    error_msg += httplib::to_string(res.error());
                    throw std::runtime_error(error_msg);
                }
                
                retry_count = 0;
                LOG_INFO("SSE thread: Connection successful");
            } catch (const std::exception& e) {                
                if (!sse_running_) {
                    LOG_INFO("SSE connection actively closed, no retry needed");
                    break;
                }
                
                if (++retry_count > max_retries) {
                    LOG_ERROR("Maximum retry count reached, stopping SSE connection attempts");
                    break;
                }

                LOG_ERROR("SSE connection error: ", e.what());
                
                int delay = retry_delay_base * (1 << (retry_count - 1));
                LOG_INFO("Will retry in ", delay, " ms (attempt ", retry_count, "/", max_retries, ")");
                
                const int check_interval = 100;
                for (int waited = 0; waited < delay && sse_running_; waited += check_interval) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(check_interval));
                }
                
                if (!sse_running_) {
                    LOG_INFO("SSE connection actively closed during retry wait, stopping retry");
                    break;
                }
            }
        }
        
        LOG_INFO("SSE thread: Exiting");
    });
}

bool sse_client::parse_sse_data(const char* data, size_t length) {
    try {
        // Split into lines and process event fields
        std::istringstream stream(std::string(data, length));
        std::string line;
        std::string event_type = "message";
        std::vector<std::string> data_lines;
        
        while (std::getline(stream, line)) {
            // Trim trailing CR if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (line.substr(0, 7) == "event: ") {
                event_type = line.substr(7);
            } else if (line.substr(0, 6) == "data: ") {
                data_lines.push_back(line.substr(6));
            } else if (line.empty()) {
                break; // End of event
            }
        }
        
        if (data_lines.empty()) {
            return true;
        }
        
        // Join data lines with newlines
        std::string data_content;
        for (size_t i = 0; i < data_lines.size(); ++i) {
            if (i > 0) data_content += '\n';
            data_content += data_lines[i];
        }
        
        if (event_type == "heartbeat") {
            return true;
        } else if (event_type == "endpoint") {
            std::lock_guard<std::mutex> lock(mutex_);
            msg_endpoint_ = resolve_endpoint_path(data_content);
            endpoint_cv_.notify_all();
            return true;
        } else if (event_type == "message") {
            try {
                json response = json::parse(data_content);
                
                if (response.contains("jsonrpc") && response.contains("id") && !response["id"].is_null()) {
                    json id = response["id"];
                    
                    std::lock_guard<std::mutex> lock(response_mutex_);
                    auto it = pending_requests_.find(id);
                    if (it != pending_requests_.end()) {
                        if (response.contains("result")) {
                            it->second.set_value(response["result"]);
                        } else if (response.contains("error")) {
                            json error_result = {
                                {"isError", true},
                                {"error", response["error"]}
                            };
                            it->second.set_value(error_result);
                        } else {
                            it->second.set_value(json::object());
                        }
                        
                        pending_requests_.erase(it);
                    } else {
                        LOG_WARNING("Received response for unknown request ID: ", id);
                    }
                } else {
                    LOG_WARNING("Received invalid JSON-RPC response: ", response.dump());
                }
            } catch (const json::exception& e) {
                LOG_ERROR("Failed to parse JSON-RPC response: ", e.what());
            }
            return true;
        } else {
            LOG_WARNING("Received unknown event type: ", event_type);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing SSE data: ", e.what());
        return false;
    }
}

void sse_client::close_sse_connection() {
    if (!sse_running_) {
        LOG_INFO("SSE connection already closed");
        return;
    }
    
    LOG_INFO("Actively closing SSE connection (normal exit flow)...");
    
    sse_running_ = false;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (sse_thread_ && sse_thread_->joinable()) {
        auto timeout = std::chrono::seconds(5);
        auto start = std::chrono::steady_clock::now();
        
        LOG_INFO("Waiting for SSE thread to end...");
        
        while (sse_thread_->joinable() && 
            std::chrono::steady_clock::now() - start < timeout) {
            try {
                sse_thread_->join();
                LOG_INFO("SSE thread successfully ended");
                break;
            } catch (const std::exception& e) {
                LOG_ERROR("Error waiting for SSE thread: ", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        if (sse_thread_->joinable()) {
            LOG_WARNING("SSE thread did not end within timeout, detaching thread");
            sse_thread_->detach();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        msg_endpoint_.clear();
        endpoint_cv_.notify_all();
    }
    
    LOG_INFO("SSE connection successfully closed (normal exit flow)");
}

json sse_client::send_jsonrpc(const request& req) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (msg_endpoint_.empty()) {
        throw mcp_exception(error_code::internal_error, "Message endpoint not set, SSE connection may not be established");
    }
    
    json req_json = req.to_json();
    std::string req_body = req_json.dump();
    
    httplib::Headers headers;
    headers.emplace("Content-Type", "application/json");
    
    for (const auto& [key, value] : default_headers_) {
        headers.emplace(key, value);
    }
    
    if (req.is_notification()) {
        auto result = http_client_->Post(msg_endpoint_, headers, req_body, "application/json");
        
        if (!result) {
            auto err = result.error();
            std::string error_msg = httplib::to_string(err);
            LOG_ERROR("JSON-RPC request failed: ", error_msg);
            throw mcp_exception(error_code::internal_error, error_msg);
        }
        
        return json::object();
    }
    
    std::promise<json> response_promise;
    std::future<json> response_future = response_promise.get_future();
    
    {
        std::lock_guard<std::mutex> response_lock(response_mutex_);
        pending_requests_[req.id] = std::move(response_promise);
    }
    
    auto result = http_client_->Post(msg_endpoint_, headers, req_body, "application/json");
    
    if (!result) {
        auto err = result.error();
        std::string error_msg = httplib::to_string(err);
        
        {
            std::lock_guard<std::mutex> response_lock(response_mutex_);
            pending_requests_.erase(req.id);
        }
        
        LOG_ERROR("JSON-RPC request failed: ", error_msg);
        throw mcp_exception(error_code::internal_error, error_msg);
    }
    
    if (result->status / 100 != 2) {
        try {
            json res_json = json::parse(result->body);
            
            {
                std::lock_guard<std::mutex> response_lock(response_mutex_);
                pending_requests_.erase(req.id);
            }
            
            if (res_json.contains("error")) {
                int code = res_json["error"]["code"];
                std::string message = res_json["error"]["message"];
                
                throw mcp_exception(static_cast<error_code>(code), message);
            }
            
            if (res_json.contains("result")) {
                return res_json["result"];
            } else {
                return json::object();
            }
        } catch (const json::exception& e) {
            {
                std::lock_guard<std::mutex> response_lock(response_mutex_);
                pending_requests_.erase(req.id);
            }
            
            throw mcp_exception(error_code::parse_error, 
                            "Failed to parse JSON-RPC response: " + std::string(e.what()));
        }
    } else {
        const auto timeout = std::chrono::seconds(timeout_seconds_);
        
        auto status = response_future.wait_for(timeout);
        
        if (status == std::future_status::ready) {
            json response = response_future.get();
            
            if (response.contains("isError") && response["isError"].is_boolean() && response["isError"].get<bool>()) {
                if (response.contains("error") && response["error"].is_object()) {
                    const auto& err_obj = response["error"];
                    int code = err_obj.contains("code") ? err_obj["code"].get<int>() : static_cast<int>(error_code::internal_error);
                    std::string message = err_obj.value("message", "");
                    // Handle error
                    throw mcp_exception(static_cast<error_code>(code), message);
                }
            }
            
            return response;
        } else {
            {
                std::lock_guard<std::mutex> response_lock(response_mutex_);
                pending_requests_.erase(req.id);
            }
            
            throw mcp_exception(error_code::internal_error, "Timeout waiting for SSE response");
        }
    }
}

bool sse_client::is_running() const {
    return sse_running_;
}

} // namespace mcp
