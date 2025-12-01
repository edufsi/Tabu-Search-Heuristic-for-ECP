// args.cpp
#include "args.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring> // strcmp

static void expect_value(int i, int argc, const char* arg_name) {
    if (i + 1 >= argc) {
        throw std::runtime_error(std::string("Missing value for argument: ") + arg_name);
    }
}

Arguments parse_arguments(int argc, char** argv) {
    if (argc < 3) {
        throw std::runtime_error(
            "Usage: ./eqcol <input_file> <output_file> [options]\n"
        );
    }

    Arguments args;

    args.output_file = argv[1];
    args.input_file = argv[2];
    

    for (int i = 3; i < argc; i++) {
        auto eq = [&](const char* s){ return strcmp(argv[i], s) == 0; };

        if (eq("--seed")) {
            expect_value(i, argc, "--seed");
            args.seed = std::stoi(argv[++i]);
        }
        else if (eq("--alpha")) {
            expect_value(i, argc, "--alpha");
            args.alpha = std::stod(argv[++i]);
        }
        else if (eq("--beta")) {
            expect_value(i, argc, "--beta");
            args.beta = std::stod(argv[++i]);
        }

        else if (eq("--aspiration")) {
            expect_value(i, argc, "--aspiration");
            args.aspiration = std::stoi(argv[++i]);
            if (args.aspiration != 0 && args.aspiration != 1) {
                throw std::runtime_error("--aspiration must be 0 or 1");
            }
        }
        else if (eq("--time_limit")) {
            expect_value(i, argc, "--time_limit");
            args.time_limit = std::stoi(argv[++i]);
        }
        else if (eq("--max_iter")) {
            expect_value(i, argc, "--max_iter");
            args.max_iter = std::stoi(argv[++i]);
        }
        else if (eq("--perturbation_limit")) {
            expect_value(i, argc, "--perturbation_limit");
            args.perturbation_limit = std::stoi(argv[++i]);
        }
        else if (eq("--perturbation_strength")) {
            expect_value(i, argc, "--perturbation_strength");
            args.perturbation_strength = std::stof(argv[++i]);
        }
        else {
            throw std::runtime_error(std::string("Unknown argument: ") + argv[i]);
        }
    }


    return args;
}
