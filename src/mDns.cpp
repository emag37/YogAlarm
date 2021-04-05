#include "mDns.hpp"

#include "mdns.h"

namespace mDns{
    void AddHttpService(const std::string& hostname, const std::string& instance_name) {
        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(hostname.c_str()));
        ESP_ERROR_CHECK(mdns_instance_name_set(instance_name.c_str()));
        ESP_ERROR_CHECK(mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0));
        //ESP_ERROR_CHECK(mdns_service_instance_name_set("_http", "_tcp", (hostname + " HTTP Access").c_str()));
    }
}