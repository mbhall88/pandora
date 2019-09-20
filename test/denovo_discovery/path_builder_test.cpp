#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "denovo_discovery/path_builder.h"

class MockPathBuilder : public PathBuilder {
public:
    MockPathBuilder(){}
};

TEST(isStartAndEndConnected, startAndEndNotConnectedReturnsFalse) {
    MockPathBuilder path_builder;
    path_builder.tree = DfsTree();

    EXPECT_FALSE(path_builder.is_start_and_end_connected());
}

TEST(isStartAndEndConnected, startAndEndAreConnectedReturnsTrue) {
    MockPathBuilder path_builder;

    EXPECT_TRUE(path_builder.is_start_and_end_connected());
}