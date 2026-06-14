// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

TEST(MathTest, Add)
{
	EXPECT_EQ(2 + 2, 4);
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}