// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <string>
#include <iostream>

#include <thread>
#include <fstream>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include "boost/program_options.hpp"

namespace onnxruntime {
namespace server {

namespace po = boost::program_options;

// Enumerates the different type of results which can occur
// The three different types are:
// 0. ExitSuccess which is when the program should exit with EXIT_SUCCESS
// 1. ExitFailure when program should exit with EXIT_FAILURE
// 2. No need for exiting the program, continue
enum class Result {
  ExitSuccess,
  ExitFailure,
  ContinueSuccess
};

static std::unordered_map<std::string, spdlog::level::level_enum> supported_log_levels{
    {"verbose", spdlog::level::level_enum::debug},
    {"info", spdlog::level::level_enum::info},
    {"warning", spdlog::level::level_enum::warn},
    {"error", spdlog::level::level_enum::err},
    {"fatal", spdlog::level::level_enum::critical}};

// Map environment variables to program options.
// CONFONNX_FOO_BAR -> foo-bar
class env_name_mapper {
 public:
  env_name_mapper(const po::options_description& desc, const std::string& prefix) : desc(desc), prefix(prefix) {}

  std::string operator()(const std::string& s) {
    std::string result;
    if (s.find(prefix) != 0) {
      return std::string();
    }
    result = s.substr(prefix.size(), std::string::npos);
    if (result.empty()) {
      return std::string();
    }
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::replace(result.begin(), result.end(), '_', '-');
    if (!desc.find_nothrow(result, false)) {
      std::cout << "Environment variable " << s << " does not match any server option, ignoring" << std::endl;
      return std::string();
    }
    std::cout << "Using environment variable " << s << " as server option --" << result << std::endl;
    return result;
  }

 private:
  const po::options_description& desc;
  std::string prefix;
};

// Wrapper around Boost program_options and should provide all the functionality for options parsing
// Provides sane default values
class ServerConfiguration {
 public:
  const std::string full_desc = "ONNX Server: host an ONNX model for inferencing with ONNX Runtime";
  std::string enclave_path = "confonnx_enclave";
  std::string model_path;
  int key_rollover_interval_seconds = 60 * 60 * 24;  // 24 h
  int key_sync_interval_seconds = 60 * 60 * 1;       // 1 h
  int key_error_retry_interval_seconds = 60 * 5;     // 5 min
  std::string address = "0.0.0.0";
  int http_port = 8001;
  std::string auth_key;
  int num_http_threads = std::thread::hardware_concurrency();
  spdlog::level::level_enum logging_level{};
  bool debug = false;
  bool simulation = false;
  bool use_akv = false;
  bool use_model_key_provisioning = false;
  std::string akv_app_id;
  std::string akv_app_pwd;
  std::string akv_vault_url;
  std::string akv_service_key_name = "confonnx-server";
  std::string akv_model_key_name;
  std::string akv_attestation_url;

  ServerConfiguration() {
    desc.add_options()("help,h", "Shows a help message and exits");
    desc.add_options()("log-level", po::value(&log_level_str)->default_value(log_level_str), "Logging level. Allowed options (case sensitive): verbose, info, warning, error, fatal");
    desc.add_options()("enclave-path", po::value(&enclave_path)->default_value(enclave_path), "Path to enclave binary");
    desc.add_options()("model-path", po::value(&model_path)->required(), "Path to ONNX model");
    desc.add_options()("address", po::value(&address)->default_value(address), "The base HTTP address");
    desc.add_options()("http-port", po::value(&http_port)->default_value(http_port), "HTTP port to listen to requests");
    desc.add_options()("auth-key", po::value(&auth_key), "Authorization key (for development without frontend server)");
    desc.add_options()("key-rollover-interval", po::value(&key_rollover_interval_seconds)->default_value(key_rollover_interval_seconds), "Key rollover interval in seconds");
    desc.add_options()("key-sync-interval", po::value(&key_sync_interval_seconds)->default_value(key_sync_interval_seconds), "Key sync interval in seconds");
    desc.add_options()("key-error-retry-interval", po::value(&key_error_retry_interval_seconds)->default_value(key_error_retry_interval_seconds), "Key rollover/sync error retry interval in seconds");
    desc.add_options()("num-http-threads", po::value(&num_http_threads)->default_value(num_http_threads), "Number of http threads");
    desc.add_options()("use-model-key-provisioning", po::bool_switch(&use_model_key_provisioning), "Provision model key via API request");
    desc.add_options()("use-akv", po::bool_switch(&use_akv), "Use Azure Key Vault for key management, required for distributed deployment of server");
    desc.add_options()("akv-app-id", po::value(&akv_app_id), "ID of Azure enterprise application used to access AKV");
    desc.add_options()("akv-app-pwd", po::value(&akv_app_pwd), "Password of Azure enterprise application used to access Azure Key Vault");
    desc.add_options()("akv-vault-url", po::value(&akv_vault_url), "URL of Azure Key Vault instance");
    desc.add_options()("akv-service-key-name", po::value(&akv_service_key_name)->default_value(akv_service_key_name), "Name of service key to use in Azure Key Vault");
    desc.add_options()("akv-model-key-name", po::value(&akv_model_key_name), "Name of model key to use in Azure Key Vault");
    desc.add_options()("akv-attestation-url", po::value(&akv_attestation_url), "URL of Azure Attestation Service used with AKV");
    desc.add_options()("debug", po::bool_switch(&debug), "Allow loading of unsigned debug enclaves");
    desc.add_options()("simulation", po::bool_switch(&simulation), "Run in simulation mode on non-SGX hardware");
  }

  // Parses argc and argv and sets the values for the class
  // Returns an enum with three options: ExitSuccess, ExitFailure, ContinueSuccess
  // ExitSuccess and ExitFailure means the program should exit but is left to the caller
  Result ParseInput(int ac, char** av) {
    try {
      po::store(po::parse_environment(desc, env_name_mapper(desc, "CONFONNX_")), vm);  // can throw
      po::store(po::command_line_parser(ac, av).options(desc).run(), vm);        // can throw

      if (vm.count("help") || vm.count("h")) {
        PrintHelp(std::cout, full_desc);
        return Result::ExitSuccess;
      }

      po::notify(vm);  // throws on error, so do after help
    } catch (const po::error& e) {
      PrintHelp(std::cerr, e.what());
      return Result::ExitFailure;
    } catch (const std::exception& e) {
      PrintHelp(std::cerr, e.what());
      return Result::ExitFailure;
    }

    Result result = ValidateOptions();

    if (result == Result::ContinueSuccess) {
      logging_level = supported_log_levels[log_level_str];
    }

    return result;
  }

 private:
  po::options_description desc{"Allowed options"};
  po::variables_map vm{};
  std::string log_level_str = "info";

  // Print help and return if there is a bad value
  Result ValidateOptions() {
    if (supported_log_levels.find(log_level_str) == supported_log_levels.end()) {
      PrintHelp(std::cerr, "--log-level must be one of verbose, info, warning, error, or fatal");
      return Result::ExitFailure;
    }
    if (num_http_threads <= 0) {
      PrintHelp(std::cerr, "--num-http-threads must be greater than 0");
      return Result::ExitFailure;
    }
    if (!file_exists(enclave_path)) {
      PrintHelp(std::cerr, "--enclave-path must be the location of a valid file");
      return Result::ExitFailure;
    }
    if (!file_exists(model_path)) {
      PrintHelp(std::cerr, "--model-path must be the location of a valid file");
      return Result::ExitFailure;
    }
    if (use_akv && (akv_app_id.empty() || akv_app_pwd.empty() || akv_vault_url.empty())) {
      PrintHelp(std::cerr, "--use-akv requires --akv-*");
      return Result::ExitFailure;
    }
    if (use_model_key_provisioning && !akv_model_key_name.empty()) {
      PrintHelp(std::cerr, "--use-model-key-provisioning cannot be used with --akv-model-key-name");
      return Result::ExitFailure;
    }
    return Result::ContinueSuccess;
  }

  // Prints a helpful message (param: what) to the user and then the program options
  // Example: config.PrintHelp(std::cout, "Non-negative values not allowed")
  // Which will print that message and then all publicly available options
  void PrintHelp(std::ostream& out, const std::string& what) const {
    out << what << std::endl
        << desc << std::endl;
  }

  inline bool file_exists(const std::string& fileName) {
    std::ifstream infile(fileName.c_str());
    return infile.good();
  }
};

}  // namespace server
}  // namespace onnxruntime
