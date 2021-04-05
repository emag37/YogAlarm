#pragma once

#include <memory>
#include <functional>
#include <vector>

#include "esp_http_server.h"
#include "DataBinding.hpp"

class WebUI {
    struct RequestHandler {
        std::string uri;
        httpd_method_t method;
        std::function<esp_err_t(httpd_req_t*)> handler_fn;
    };

    std::vector<RequestHandler> _registered_handlers;
    httpd_config_t _config;
    httpd_handle_t _handle;
    std::shared_ptr<DataBinding<double>> _temperature_source;
    std::shared_ptr<DataBinding<std::pair<double, double>>> _alarm_threshold_binding;

    static esp_err_t HandleRequest(httpd_req_t *req);

    esp_err_t HandleGetForm(httpd_req_t *req);
    esp_err_t HandleGetTemp(httpd_req_t *req);
    esp_err_t HandleGetThresholds(httpd_req_t *req);
    esp_err_t HandlePost(httpd_req_t *req);

    void RegisterHandler(httpd_method_t type, const std::string& uri, const std::function<esp_err_t(httpd_req_t*)>& handler);
    
    static std::vector<std::pair<std::string, std::string>> ParseJsonKeyValuePairs(const std::string& json);
public:
    WebUI(const std::shared_ptr<DataBinding<double>>& temperature_source,
    const std::shared_ptr<DataBinding<std::pair<double, double>>>& alarm_threshold_binding);
    ~WebUI();
};