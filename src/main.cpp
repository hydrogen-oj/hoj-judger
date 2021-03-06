/*
    This file is part of the Hydrogen OJ Judger.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/* Copyright (C) 2020-2021 Alex Cui */

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "../third_party/argparse/argparse.h"
#include "../third_party/yaml-cpp/include/yaml-cpp/yaml.h"

#include "result.h"
#include "run.h"
#include "util.h"

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    argparse::ArgumentParser parser("hoj-judger", "Judger for hydrogen oj");
    parser.add_argument("-v", "--version", "show version", false);
    parser.add_argument("-s", "--source", "set source file", true);
    parser.add_argument("-l", "--language", "set judger language", true);
    parser.add_argument("-p", "--problem", "set problem directory", true);
    parser.add_argument("-o", "--output", "set output directory", false);
    parser.add_argument("-c", "--checker", "set checker path", false);
    
    parser.enable_help();
    
    auto err = parser.parse(argc, const_cast<const char **>(argv));
    if (err) {
        if (parser.exists("version")) {
            std::cout << "v0.2.0" << std::endl;
            return 0;
        }
        std::cout << err << std::endl;
        return -1;
    }

    if (parser.exists("help")) {
        parser.print_help();
        return 0;
    }
    
    fs::path judger_config_path = "./hoj-judger-config.yml";
    fs::path problem_path = parser.get<std::string>("problem");
    fs::path problem_config_path = problem_path / "config.yml";
    fs::path source_path = parser.get<std::string>("source");
    fs::path output_path = parser.exists("output") ? parser.get<std::string>("output") : ".";
    fs::path compile_log_path = output_path / "compile.log";
    fs::path result_path = output_path / "result.yml";
    fs::path default_checker_path = parser.exists("checker") ? parser.get<std::string>("checker") : "./hoj-checker";
    std::string language = parser.get<std::string>("language");

    std::cout << "Config Path: " << judger_config_path << std::endl;
    std::cout << "Problem Directory: " << problem_config_path << std::endl;
    std::cout << "Language: " << language << std::endl;

    YAML::Node judger_config = YAML::LoadFile(judger_config_path);
    YAML::Node problem_config = YAML::LoadFile(problem_config_path);
    YAML::Node result;

    std::ofstream result_file(result_path);
    if (!result_file.is_open()) {
        return -1;
    }

    YAML::Node language_config = judger_config["language"][language];
    if (language_config) {
        YAML::Node compile_config = language_config["compile"];
        YAML::Node run_config = language_config["run"];

        std::map<std::string, std::string> data;
        data["source"] = source_path;
        data["executable"] = output_path / "exe";

        if (compile_config) {
            int max_compile_time = 10000;
            int max_compile_space = 262144;

            std::string compile = hoj::format(compile_config.as<std::string>(), data);
            
            std::cout << "Compile: " << compile << std::endl;

            hoj::resource_usage usage;
            int compile_status = hoj::compile(usage, compile, compile_log_path, max_compile_time, max_compile_space);
            
            if (usage.real_time > max_compile_time) { // CTLE
                result["status"] = static_cast<int>(hoj::judge_status::COMPILE_ERROR);
                result["space"] = 0;
                result["time"] = 0;
                result["score"] = 0;

                int test_count = problem_config["case_count"].as<int>();
                result["case_count"] = test_count;
                for (int i = 0; i < test_count; ++i) {
                    result["case"][i]["space"] = 0;
                    result["case"][i]["time"] = 0;
                    result["case"][i]["score"] = 0;
                    result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::COMPILE_TIME_LIMIT_EXCEEDED);
                }

                result_file << result;
                result_file.close();
                return 0;
            }
            else if (usage.memory > max_compile_space) { // CMLE
                result["status"] = static_cast<int>(hoj::judge_status::COMPILE_ERROR);

                int test_count = problem_config["case_count"].as<int>();
                result["case_count"] = test_count;
                for (int i = 0; i < test_count; ++i) {
                    result["case"][i]["space"] = 0;
                    result["case"][i]["time"] = 0;
                    result["case"][i]["score"] = 0;
                    result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::COMPILE_MEMORY_LIMIT_EXCEEDED);
                }

                result_file << result;
                result_file.close();
                return 0;
            }
            else if (compile_status != 0) { // CE
                std::ifstream compile_log(compile_log_path);
                if (!compile_log.is_open()) { // UKE
                    result["status"] = static_cast<int>(hoj::judge_status::UNKNOWN_ERROR);
                    int test_count = problem_config["case_count"].as<int>();
                    result["case_count"] = test_count;
                    for (int i = 0; i < test_count; ++i) {
                        result["case"][i]["space"] = 0;
                        result["case"][i]["time"] = 0;
                        result["case"][i]["score"] = 0;
                        result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::UNKNOWN_ERROR);
                    }
                    return 0;
                }
                
                std::string compile_detail, temp;
                while (!compile_log.eof()) {
                    getline(compile_log, temp);
                    temp.push_back('\n');
                    compile_detail += temp;
                }

                result["info"] = compile_detail;
                result["status"] = static_cast<int>(hoj::judge_status::COMPILE_ERROR);
                
                int test_count = problem_config["case_count"].as<int>();
                result["case_count"] = test_count;
                for (int i = 0; i < test_count; ++i) {
                    result["case"][i]["space"] = 0;
                    result["case"][i]["time"] = 0;
                    result["case"][i]["score"] = 0;
                    result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::COMPILE_ERROR);
                }

                result_file << result;
                result_file.close();
                return 0;
            }
        }

        if (run_config) {
            std::string run = hoj::format(run_config.as<std::string>(), data);
            std::cout << "Run: " << run << std::endl;
            
            std::string name = problem_config["file"].as<std::string>();
            std::string mode = problem_config["mode"].as<std::string>();
            std::string type = problem_config["type"].as<std::string>();

            int test_count = problem_config["case_count"].as<int>();
            YAML::Node test_case_list = problem_config["case"];
            result["case_count"] = test_count;

            int total_score = 0;
            int total_time = 0;
            int total_space = 0;

            hoj::judge_status final_status = hoj::judge_status::ACCEPTED;

            for (int i = 0; i < test_count; ++i) {
                YAML::Node test_case = test_case_list[i];
                int max_time = test_case["time"].as<int>();
                int max_space = test_case["space"].as<int>();
                int score = test_case["score"].as<int>();

                fs::path input = problem_path / (name + std::to_string(i + 1) + ".in");
                fs::path output = output_path / "output.out";
                fs::path answer = problem_path / (name + std::to_string(i + 1) + ".out");

                hoj::resource_usage usage;
                int status = hoj::run(usage, run, input, output, max_time, max_space);
                result["case"][i]["space"] = usage.memory;
                result["case"][i]["time"] = usage.real_time;

                total_space = std::max(total_space, usage.memory);
                total_time += usage.real_time;
            
                if (usage.real_time > max_time) {
                    result["case"][i]["score"] = 0;
                    result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::TIME_LIMIT_EXCEEDED);
                    final_status = hoj::judge_status::UNACCEPTED;
                }
                else if (usage.memory > max_space) {
                    result["case"][i]["score"] = 0;
                    result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::MEMORY_LIMIT_EXCEEDED);
                    final_status = hoj::judge_status::UNACCEPTED;
                }
                else if (status != 0) {
                    result["case"][i]["score"] = 0;
                    result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::RUNTIME_ERROR);
                    final_status = hoj::judge_status::UNACCEPTED;
                }
                else {
                    int status = 0;
                    if (type == "default") {
                        hoj::resource_usage usage;
                        status = hoj::check(usage, default_checker_path, input, output, answer, 1000, 1000000); // TODO LIMIT
                    }
                    else if (type == "spj") {
                        // TODO: SPJ
                    }
                    else {
                        // TODO: ERR CONFIG
                        return -3;
                    }

                    switch (WEXITSTATUS(status)) {
                        case 0: { // AC
                            result["case"][i]["score"] = score;
                            result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::ACCEPTED);
                            total_score += score;
                            break;
                        }
                        case 1: { // WA
                            result["case"][i]["score"] = 0;
                            result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::WRONG_ANSWER);
                            final_status = hoj::judge_status::UNACCEPTED;
                            break;
                        }
                        case 2: { // PE
                            result["case"][i]["score"] = 0;
                            result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::PRESENTATION_ERROR);
                            final_status = hoj::judge_status::UNACCEPTED;
                            break;
                        }
                        default: { // UKE
                            result["case"][i]["score"] = 0;
                            result["case"][i]["status"] = static_cast<int>(hoj::test_case_status::UNKNOWN_ERROR);
                            break;
                        }
                    }
                }

                std::cout << "Test Case " << i + 1 << ": "
                    << result["case"][i]["time"] << "ms "
                    << result["case"][i]["space"] << "KiB "
                    << result["case"][i]["score"] << "pts "
                    << result["case"][i]["status"] << std::endl;
            }

            result["score"] = total_score;
            result["time"] = total_time;
            result["space"] = total_space;
            result["status"] = static_cast<int>(final_status);
            result_file << result;
            result_file.close();

            std::cout << "Judge Finished." << std::endl;
        }
        else {
            return -2;
        }
    }

    return 0;
}
