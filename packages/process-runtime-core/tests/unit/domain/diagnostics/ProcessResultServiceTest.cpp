#include "domain/diagnostics/domain-services/ProcessResultService.h"
#include "domain/diagnostics/domain-services/ProcessResultSerialization.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Domain::Diagnostics::Aggregates::TestRecord;
using Siligen::Domain::Diagnostics::Aggregates::TestStatus;
using Siligen::Domain::Diagnostics::Aggregates::TestType;
using Siligen::Domain::Diagnostics::DomainServices::ProcessResultService;
using Siligen::Domain::Diagnostics::Serialization::SerializeJogParameters;
using Siligen::Domain::Diagnostics::Serialization::SerializeJogResult;
using Siligen::Domain::Diagnostics::Serialization::DeserializeJogTestData;
using Siligen::Domain::Diagnostics::ValueObjects::JogTestData;
using Siligen::Domain::Motion::ValueObjects::JogDirection;

JogTestData BuildValidJogData() {
    JogTestData data;
    data.axis = LogicalAxisId::X;
    data.direction = JogDirection::Positive;
    data.speed = 10.0;
    data.distance = 5.0;
    data.startPosition = 0.0;
    data.endPosition = 5.0;
    data.actualDistance = 5.0;
    data.responseTimeMs = 12;
    return data;
}

}  // namespace

TEST(ProcessResultSerializationTest, JogRoundTrip) {
    auto data = BuildValidJogData();

    auto params_result = SerializeJogParameters(data);
    ASSERT_TRUE(params_result.IsSuccess());

    auto result_result = SerializeJogResult(data);
    ASSERT_TRUE(result_result.IsSuccess());

    auto round_trip = DeserializeJogTestData(params_result.Value(), result_result.Value());
    ASSERT_TRUE(round_trip.IsSuccess());

    const auto& restored = round_trip.Value();
    EXPECT_EQ(restored.axis, data.axis);
    EXPECT_EQ(restored.direction, data.direction);
    EXPECT_DOUBLE_EQ(restored.speed, data.speed);
    EXPECT_DOUBLE_EQ(restored.distance, data.distance);
    EXPECT_DOUBLE_EQ(restored.startPosition, data.startPosition);
    EXPECT_DOUBLE_EQ(restored.endPosition, data.endPosition);
    EXPECT_DOUBLE_EQ(restored.actualDistance, data.actualDistance);
    EXPECT_EQ(restored.responseTimeMs, data.responseTimeMs);
}

TEST(ProcessResultSerializationTest, JogMissingFieldFails) {
    const std::string params_json = R"({"direction":"Positive","speed":10.0,"distance":5.0})";
    const std::string result_json =
        R"({"startPosition":0.0,"endPosition":5.0,"actualDistance":5.0,"responseTimeMs":12})";

    auto round_trip = DeserializeJogTestData(params_json, result_json);
    ASSERT_TRUE(round_trip.IsError());
    EXPECT_EQ(round_trip.GetError().GetCode(), ErrorCode::JSON_MISSING_REQUIRED_FIELD);
}

TEST(ProcessResultServiceTest, BuildJogRecordSetsTypeAndTimestamp) {
    TestRecord base;
    base.operatorName = "tester";
    base.status = TestStatus::Success;
    base.durationMs = 120;
    base.timestamp = 0;

    auto record_result = ProcessResultService::BuildJogRecord(base, BuildValidJogData());
    ASSERT_TRUE(record_result.IsSuccess());

    const auto& record = record_result.Value();
    EXPECT_EQ(record.testType, TestType::Jog);
    EXPECT_GT(record.timestamp, 0);
    EXPECT_FALSE(record.parametersJson.empty());
    EXPECT_FALSE(record.resultJson.empty());
}

TEST(ProcessResultServiceTest, BuildJogRecordRejectsInvalidData) {
    TestRecord base;
    base.operatorName = "tester";
    base.status = TestStatus::Success;
    base.durationMs = 10;
    base.timestamp = 0;

    JogTestData invalid;
    invalid.axis = LogicalAxisId::X;
    invalid.direction = JogDirection::Positive;
    invalid.speed = 0.0;
    invalid.distance = 0.0;

    auto record_result = ProcessResultService::BuildJogRecord(base, invalid);
    ASSERT_TRUE(record_result.IsError());
    EXPECT_EQ(record_result.GetError().GetCode(), ErrorCode::ValidationFailed);
}
