#include "codec/wire.hpp"

#include <gtest/gtest.h>

namespace {

using namespace slipstream::codec;

TEST(Wire, HeaderIsFourBytes) { EXPECT_EQ(header::kSize, 4u); }

TEST(Wire, BodySizesMatchFieldTables) {
    EXPECT_EQ(quote::kSize, 44u);
    EXPECT_EQ(trade::kSize, 41u);  // spec prose says 49
    EXPECT_EQ(heartbeat::kSize, 8u);
    EXPECT_EQ(session_control::kSize, 9u);
    EXPECT_EQ(new_order::kSize, 50u);  // spec prose says 42
    EXPECT_EQ(exec_report::kSize, 30u);
}

TEST(Wire, ExpectedBodySizeByType) {
    EXPECT_EQ(expected_body_size(MsgType::Quote), quote::kSize);
    EXPECT_EQ(expected_body_size(MsgType::Trade), trade::kSize);
    EXPECT_EQ(expected_body_size(MsgType::Heartbeat), heartbeat::kSize);
    EXPECT_EQ(expected_body_size(MsgType::SessionControl), session_control::kSize);
    EXPECT_EQ(expected_body_size(MsgType::NewOrder), new_order::kSize);
    EXPECT_EQ(expected_body_size(MsgType::ExecReport), exec_report::kSize);
}

TEST(Wire, UnknownTypeHasNoBodySize) {
    EXPECT_EQ(expected_body_size(static_cast<MsgType>(99)), 0u);
}

}  // namespace
