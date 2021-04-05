#pragma once

#include <mutex>
#include <type_traits>
#include <stdio.h>

template <typename T>
class DataBinding {
public:
    virtual void SetValue(T value) = 0;

    [[nodiscard]] virtual T GetValue() const = 0;

    virtual ~DataBinding() = default;
};

template <typename T>
class DataSourceSingleValue : public DataBinding<T> {
    T _current_value;
    mutable std::mutex _critical_section;

public:
    explicit DataSourceSingleValue(T initial_value): _current_value(initial_value) {}

    void SetValue(T value) override {
        std::lock_guard<decltype(_critical_section)> lock(_critical_section);
        _current_value = value;
    }

    [[nodiscard]] T GetValue() const override {
        std::lock_guard<decltype(_critical_section)> lock(_critical_section);
        return _current_value;
    }

    ~DataSourceSingleValue() override = default;
};