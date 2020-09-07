// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <exception>
#include <typeinfo>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/fmt/ostr.h>

#include "server/host/core/http_server.h"
#include "server/host/environment.h"
#include "server/host/request_handler.h"
#include "server/host/server_configuration.h"
#include "server/host/enclave.h"
#include "server/shared/request_type.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace server = onnxruntime::server;

int main(int argc, char* argv[]) {
  server::ServerConfiguration config{};
  auto res = config.ParseInput(argc, argv);

  if (res == server::Result::ExitSuccess) {
    exit(EXIT_SUCCESS);
  } else if (res == server::Result::ExitFailure) {
    exit(EXIT_FAILURE);
  }

  const auto env = std::make_shared<server::ServerEnvironment>(config.logging_level,
                                                               spdlog::sinks_init_list{
                                                                   std::make_shared<spdlog::sinks::stdout_sink_mt>(),
                                                                   std::make_shared<spdlog::sinks::syslog_sink_mt>()},
                                                               config.auth_key);
  auto logger = env->GetAppLogger();
  logger->debug("Logging manager initialized.");
  logger->info("Enclave path: {}", config.enclave_path);
  logger->info("Model path: {}", config.model_path);
  if (env->IsAuthEnabled()) {
    logger->info("Authorization enabled.");
  }

  std::chrono::seconds key_rollover_interval{config.key_rollover_interval_seconds};
  std::chrono::seconds key_sync_interval{config.key_sync_interval_seconds};
  std::chrono::seconds key_error_retry_interval{config.key_error_retry_interval_seconds};

  try {
    server::KeyVaultConfig service_kvc(config.akv_app_id, config.akv_app_pwd, config.akv_vault_url, config.akv_service_key_name, config.akv_attestation_url);
    server::KeyVaultConfig model_kvc(config.akv_app_id, config.akv_app_pwd, config.akv_vault_url, config.akv_model_key_name);

    server::Enclave enclave(config.enclave_path, config.debug, config.simulation, env,
                            std::move(service_kvc), std::move(model_kvc), config.use_model_key_provisioning,
                            key_rollover_interval, key_sync_interval, key_error_retry_interval);
    enclave.Initialize(config.model_path, env);

    auto const boost_address = boost::asio::ip::make_address(config.address);
    server::App app;

    app.RegisterStartup(
        [&env](const auto& details) -> void {
          auto logger = env->GetAppLogger();
          logger->info("Listening at: http://{}:{}", details.address.to_string(), details.port);
        });

    app.RegisterError(
        [&env](auto& context) -> void {
          auto logger = env->GetLogger(context.request_id);
          logger->debug("Error code: {}", context.error_code);
          logger->debug("Error message: {}", context.error_message);

          context.response.result(context.error_code);
          context.response.insert("Content-Type", "application/json");
          context.response.insert("x-ms-request-id", context.request_id);
          if (!context.client_request_id.empty()) {
            context.response.insert("x-ms-client-request-id", (context).client_request_id);
          }
          context.response.body() = server::CreateJsonError(-1, context.error_message);
        });

    app.RegisterPost(
        R"(/score)",
        [&env, &enclave](auto& context) -> void {
          server::HandleRequest(context, RequestType::Score, enclave, env);
        });

    app.RegisterPost(
        R"(/provisionModelKey)",
        [&env, &enclave](auto& context) -> void {
          server::HandleRequest(context, RequestType::ProvisionModelKey, enclave, env);
        });

    app.Bind(boost_address, config.http_port)
        .NumThreads(config.num_http_threads)
        .Run();
  } catch (std::exception& exc) {
    std::string name = typeid(exc).name();
    logger->critical("ERROR ({}): {}", name, exc.what());
    exit(EXIT_FAILURE);
  } catch (...) {
    logger->critical("Unknown error occurred");
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
