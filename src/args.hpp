// args.hpp
#pragma once
#include <string>
#include <optional>

struct Arguments {
    std::string input_file;
    std::string output_file;

    int k;
    int seed = 0;

    double alpha = 0.6;
    double beta = 0.4;

    std::string tenure_type = "randomized"; // or "fixed"

    int aspiration = 0; // 0 = off, 1 = on
    int time_limit = 1000; // seconds
    int max_iter = 10000000;
    int perturbation_limit = 1000; // iterations without improvement before perturbation
};

Arguments parse_arguments(int argc, char** argv);
