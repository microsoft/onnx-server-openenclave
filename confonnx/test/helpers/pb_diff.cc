// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <string>
#include <iostream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "google/protobuf/util/message_differencer.h"
#pragma GCC diagnostic pop

#include "google/protobuf/io/zero_copy_stream_impl.h"

#include "test/helpers/pb_diff.h"

namespace pb = google::protobuf;

namespace onnxruntime {
namespace server {
namespace test {

bool ProtobufCompare(const pb::Message& a, const pb::Message& b) {
  pb::util::MessageDifferencer differ;
  pb::io::OstreamOutputStream ostream(&std::cout);
  pb::util::MessageDifferencer::StreamReporter reporter(&ostream);
  differ.ReportDifferencesTo(&reporter);
  pb::util::DefaultFieldComparator comparator;
  comparator.set_float_comparison(pb::util::DefaultFieldComparator::FloatComparison::APPROXIMATE);
  differ.set_field_comparator(&comparator);
  return differ.Compare(a, b);
}

}  // namespace test
}  // namespace server
}  // namespace onnxruntime
