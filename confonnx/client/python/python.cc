// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <pybind11/pybind11.h>
#include "confmsg/client/api.h"

namespace py = pybind11;

#define MAX_KEY_REQUEST_SIZE 100 * 1024    // 100 KiB
#define MAX_REQUEST_SIZE 10 * 1024 * 1024  // 10 MiB

PYBIND11_MODULE(PY_MODULE_NAME, m) {  // NOLINT
  py::class_<confmsg::Client>(m, "Client")
      .def(py::init([](const std::string& enclave_signing_key_pem,
                       const std::string& enclave_hash,
                       const std::string& enclave_service_id,
                       bool allow_debug,
                       bool verbose) {
        auto key_provider = confmsg::RandomKeyProvider::Create(KEY_SIZE);
        std::vector<uint8_t> enclave_hash_bytes = confmsg::Hex2Buffer(enclave_hash);
        std::vector<uint8_t> enclave_service_id_bytes = confmsg::Hex2Buffer(enclave_service_id);
        return confmsg::Client(std::move(key_provider),
                               enclave_signing_key_pem,
                               enclave_hash_bytes,
                               enclave_service_id_bytes,
                               allow_debug,
                               verbose);
      }))
      .def("make_key_request", [](confmsg::Client& c) {
        std::vector<uint8_t> msg(MAX_KEY_REQUEST_SIZE);
        size_t msg_size;
        c.MakeKeyRequest(msg.data(), &msg_size, msg.size());
        return py::bytes(reinterpret_cast<char*>(msg.data()), msg_size);
      })
      .def("make_request", [](confmsg::Client& c, const std::string& data) {
        std::vector<uint8_t> msg(MAX_REQUEST_SIZE);
        size_t msg_size;
        c.MakeRequest(confmsg::CBuffer(data), msg.data(), &msg_size, msg.size());
        return py::bytes(reinterpret_cast<char*>(msg.data()), msg_size);
      })
      .def("handle_message", [](confmsg::Client& c, const std::string& data) {
        // TODO use CBuffer for inputs
        return c.HandleMessage(reinterpret_cast<const uint8_t*>(data.data()), data.size());
      });

  py::class_<confmsg::Client::Result>(m, "ClientResult")
      .def("has_data", [](confmsg::Client::Result& r) {
        return r.IsResponse();
      })
      .def("get_data", [](confmsg::Client::Result& r) {
        auto payload = r.GetPayload();
        return py::bytes(reinterpret_cast<const char*>(payload.data()), payload.size());
      })
      .def("is_key_outdated", [](confmsg::Client::Result& r) {
        return r.IsKeyOutdated();
      });
}
