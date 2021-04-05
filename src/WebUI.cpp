#include "WebUI.hpp"

#include <sstream>
#include <algorithm>

#include "WebForm.hpp"

#include "esp_log.h"

static const char *TAG = "WebUI";

WebUI::WebUI(const std::shared_ptr<DataBinding<double>> &temperature_source,
             const std::shared_ptr<DataBinding<std::pair<double, double>>> &alarm_threshold_binding) : _config(HTTPD_DEFAULT_CONFIG()), _handle(nullptr),
                                                                                                       _temperature_source(temperature_source),
                                                                                                       _alarm_threshold_binding(alarm_threshold_binding)
{
    if (httpd_start(&_handle, &_config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Error starting web server!");
        return;
    }

    RegisterHandler(HTTP_GET, "/", [&](httpd_req_t *req) {
        return HandleGetForm(req);
    });

    RegisterHandler(HTTP_GET, "/current_temp", [&](httpd_req_t *req) {
        return HandleGetTemp(req);
    });

    RegisterHandler(HTTP_GET, "/thresholds", [&](httpd_req_t *req) {
        return HandleGetThresholds(req);
    });

    RegisterHandler(HTTP_POST, "/thresholds", [&](httpd_req_t *req) {
        return HandlePost(req);
    });
}

WebUI::~WebUI()
{
    if (_handle != nullptr)
    {
        httpd_stop(&_handle);
    }
}

esp_err_t WebUI::HandleGetForm(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Processing GET form request");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_send(req, gz_compressed_form, sizeof(gz_compressed_form));
    return ESP_OK;
}

esp_err_t WebUI::HandleGetTemp(httpd_req_t *req)
{
    const auto temp_string = std::to_string(_temperature_source->GetValue());
    return httpd_resp_send(req, temp_string.c_str(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebUI::HandleGetThresholds(httpd_req_t *req)
{
    const auto low_hi_values = _alarm_threshold_binding->GetValue();
    std::stringstream ss;
    ss << "{\"low\":\"";
    ss << low_hi_values.first;
    ss << "\", \"high\":\"";
    ss << low_hi_values.second;
    ss << "\"}";

    return httpd_resp_send(req, ss.str().c_str(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebUI::HandlePost(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Handling POST to set thresholds...");
    std::array<char, 256> body_buf;

    const size_t recv_size = std::min(req->content_len, body_buf.size());

    const int ec = httpd_req_recv(req, body_buf.data(), recv_size);
    if (ec <= 0)
    {
        ESP_LOGE(TAG, "Failed to receive POSTed data ! %d", ec);
        return ESP_FAIL;
    }

    const auto parsed_json = ParseJsonKeyValuePairs(std::string(body_buf.data()));

    if (parsed_json.size() != 2)
    {
        ESP_LOGE(TAG, "Expected 2 key-value pairs, got: %d", parsed_json.size());
    }

    double low_thresh = -300.;
    double high_thresh = low_thresh;

    for (const auto &k_v : parsed_json)
    {
        if (k_v.first == "low")
        {
            low_thresh = std::stod(k_v.second);
        }
        else if (k_v.first == "high")
        {
            high_thresh = std::stod(k_v.second);
        }
    }

    if (low_thresh > -300. && high_thresh > -300.)
    {
        ESP_LOGI(TAG, "Setting low,high alarm thresholds to %lf, %lf", low_thresh, high_thresh);
        _alarm_threshold_binding->SetValue(std::make_pair(low_thresh, high_thresh));
        return httpd_resp_send(req, "", 0);
    }

    ESP_LOGE(TAG, "Could not read low, high thresholds. Not updating");

    return httpd_resp_send_err(req, httpd_err_code_t::HTTPD_400_BAD_REQUEST, nullptr);
}

void WebUI::RegisterHandler(httpd_method_t type, const std::string &uri, const std::function<esp_err_t(httpd_req_t *)> &handler)
{
    httpd_uri_t uri_handle;
    uri_handle.uri = uri.c_str();
    uri_handle.method = type;
    uri_handle.user_ctx = this;
    uri_handle.handler = &WebUI::HandleRequest;

    _registered_handlers.push_back({uri, type, handler});
    httpd_register_uri_handler(_handle, &uri_handle);
}

std::vector<std::pair<std::string, std::string>> WebUI::ParseJsonKeyValuePairs(const std::string &json)
{
    if (json.empty() || json[0] != '{')
    {
        ESP_LOGE(TAG, "Cannot parse json: %s", json.c_str());
        return {};
    }

    ESP_LOGI(TAG, "Parsing JSON: %s", json.c_str());
    const auto remove_whitespace_quotes = [](std::string &in_str) -> std::string & {
        auto next_it = in_str.begin();

        while (next_it != in_str.end())
        {
            auto next_space_it = std::find_if(next_it, in_str.end(), [](char c) {
                return std::isspace(c) || c == '"';
            });
            if (next_space_it != in_str.end())
            {
                next_space_it = in_str.erase(next_space_it);
            }
            next_it = next_space_it;
        }
        return in_str;
    };

    std::vector<std::pair<std::string, std::string>> ret_kv_pairs;
    auto index = 1;
    bool is_key_not_value = true;

    std::pair<std::string, std::string> current_kv;
    while (true)
    {
        auto next_index = json.find_first_of(":,}", index);

        if (next_index == std::string::npos)
        {
            break;
        }

        auto str = json.substr(index, next_index - index);
        if (is_key_not_value)
        {
            current_kv.first = std::move(remove_whitespace_quotes(str));
            is_key_not_value = false;
        }
        else
        {
            current_kv.second = std::move(remove_whitespace_quotes(str));
            ret_kv_pairs.push_back(std::move(current_kv));
            current_kv = std::make_pair("", "");
            is_key_not_value = true;
        }
        index = next_index + 1;
    }

    return ret_kv_pairs;
}

esp_err_t WebUI::HandleRequest(httpd_req_t *req)
{

    auto this_obj = static_cast<WebUI *>(req->user_ctx);
    const auto &handler_list = this_obj->_registered_handlers;

    auto found_handler_it = std::find_if(handler_list.begin(), handler_list.end(), [&](const RequestHandler &handler) {
        return req->method == handler.method && handler.uri == std::string(req->uri);
    });

    if (found_handler_it == handler_list.end())
    {
        ESP_LOGE(TAG, "Could not find handler for URI %s", req->uri);
        return httpd_resp_send_404(req);
    }

    return found_handler_it->handler_fn(req);
}
