// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <iostream>
#include <unordered_set>

#include "onnx_protobuf.h"
#include "predict_protobuf.h"

#include "test/helpers/pb_loader.h"
#include "test/helpers/prettyprint.h"
#include "test/helpers/tensorproto_util.h"
#include "test/helpers/tensorproto_converter.h"

namespace onnxruntime {
namespace server {
namespace test {

namespace pb = google::protobuf;

PredictRequest TensorProtoToRequest(const ONNX_NAMESPACE::ModelProto& model,
                                    const std::vector<std::string>& paths) {
  const ONNX_NAMESPACE::GraphProto& graph = model.graph();

  // Determine all graph inputs without initializer.
  std::unordered_set<std::string> initializer_names;
  for (const auto& init : graph.initializer()) {
    if (!init.has_name()) continue;
    initializer_names.insert(init.name());
  }
  std::vector<std::string> input_names;
  for (const auto& p : graph.input()) {
    if (!p.has_name()) throw std::invalid_argument("input without name??");
    if (initializer_names.find(p.name()) == initializer_names.end()) {
      input_names.push_back(p.name());
    }
  }

  if (input_names.size() != paths.size()) {
    throw std::invalid_argument(
        "Number of graph inputs (" + std::to_string(input_names.size()) + ") " +
        "not equal to number of paths (" + std::to_string(paths.size()) + ")");
  }

  PredictRequest request;
  pb::Map<std::string, ONNX_NAMESPACE::TensorProto>* inputs = request.mutable_inputs();
  for (size_t i = 0; i < paths.size(); i++) {
    auto input_name = input_names[i];
    auto tensor = LoadProtobufFromFile<ONNX_NAMESPACE::TensorProto>(paths[i]);
    std::cout << "Input: " << input_name << " = " << paths[i] << std::endl;
    std::cout << "  Shape: " << GetTensorShapeFromTensorProto(tensor) << std::endl;
    inputs->insert({input_name, std::move(tensor)});
  }

  for (const auto& p : graph.output()) {
    std::string output_name = p.name();
    std::cout << "Output filter: " << output_name << std::endl;
    request.add_output_filter(std::move(output_name));
  }

  return request;
}

PredictResponse TensorProtoToResponse(const ONNX_NAMESPACE::ModelProto& model,
                                      const std::vector<std::string>& paths) {
  const ONNX_NAMESPACE::GraphProto& graph = model.graph();
  if (graph.output().size() != static_cast<int>(paths.size())) {
    throw std::invalid_argument(
        "Number of graph outputs (" + std::to_string(graph.output().size()) + ") " +
        "not equal to number of paths (" + std::to_string(paths.size()) + ")");
  }

  PredictResponse response;
  pb::Map<std::string, ONNX_NAMESPACE::TensorProto>* outputs = response.mutable_outputs();
  for (size_t i = 0; i < paths.size(); i++) {
    auto output_name = graph.output(i).name();
    std::cout << "Output: " << output_name << " = " << paths[i] << std::endl;
    auto tensor_proto = LoadProtobufFromFile<ONNX_NAMESPACE::TensorProto>(paths[i]);
    tensor_proto.clear_name();  // to match server output
    std::cout << "  Shape: " << GetTensorShapeFromTensorProto(tensor_proto) << std::endl;
    outputs->insert({output_name, std::move(tensor_proto)});
  }
  return response;
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
