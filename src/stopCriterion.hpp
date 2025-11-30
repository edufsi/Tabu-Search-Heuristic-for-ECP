#pragma once
#include <chrono>

struct StopCriterion {
    std::chrono::high_resolution_clock::time_point start_time;
    double max_time_seconds;
    
    StopCriterion(double time_limit) 
        : start_time(std::chrono::high_resolution_clock::now()), 
          max_time_seconds(time_limit) {}

    bool is_time_up() const {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        return elapsed.count() >= max_time_seconds;
    }

    double get_elapsed() const {
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        return elapsed.count();
    }
};