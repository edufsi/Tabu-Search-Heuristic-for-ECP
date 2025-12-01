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
    int beta = 10;
    int aspiration = 1; // 0 = off, 1 = on
    int time_limit = 1000; // seconds
    int max_iter = 1000000;
    int perturbation_limit = 1000; // iterations without improvement before perturbation
    float perturbation_strength = 0.16; // floor(perturbation_strength * n)
};

Arguments parse_arguments(int argc, char** argv);
