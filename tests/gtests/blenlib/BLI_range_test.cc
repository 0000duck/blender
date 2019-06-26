#include "testing/testing.h"
#include "BLI_range.hpp"
#include "BLI_small_vector.hpp"

using IntRange = BLI::Range<int>;
using IntVector = BLI::SmallVector<int>;

TEST(range, DefaultConstructor)
{
  IntRange range;
  EXPECT_EQ(range.size(), 0);

  IntVector vector;
  for (int value : range) {
    vector.append(value);
  }
  EXPECT_EQ(vector.size(), 0);
}

TEST(range, SingleElementRange)
{
  IntRange range(4, 5);
  EXPECT_EQ(range.size(), 1);
  EXPECT_EQ(*range.begin(), 4);

  IntVector vector;
  for (int value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 1);
  EXPECT_EQ(vector[0], 4);
}

TEST(range, MultipleElementRange)
{
  IntRange range(6, 10);
  EXPECT_EQ(range.size(), 4);

  IntVector vector;
  for (int value : range) {
    vector.append(value);
  }

  EXPECT_EQ(vector.size(), 4);
  for (uint i = 0; i < 4; i++) {
    EXPECT_EQ(vector[i], i + 6);
  }
}

TEST(range, SubscriptOperator)
{
  IntRange range(5, 10);
  EXPECT_EQ(range[0], 5);
  EXPECT_EQ(range[1], 6);
  EXPECT_EQ(range[2], 7);
}

TEST(range, Before)
{
  IntRange range = IntRange(5, 10).before(3);
  EXPECT_EQ(range[0], 2);
  EXPECT_EQ(range[1], 3);
  EXPECT_EQ(range[2], 4);
  EXPECT_EQ(range.size(), 3);
}

TEST(range, After)
{
  IntRange range = IntRange(5, 10).after(4);
  EXPECT_EQ(range[0], 10);
  EXPECT_EQ(range[1], 11);
  EXPECT_EQ(range[2], 12);
  EXPECT_EQ(range[3], 13);
  EXPECT_EQ(range.size(), 4);
}

TEST(range, ToSmallVector)
{
  IntRange range = IntRange(5, 8);
  IntVector vec = range.to_small_vector();
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 5);
  EXPECT_EQ(vec[1], 6);
  EXPECT_EQ(vec[2], 7);
}

TEST(range, Contains)
{
  IntRange range = IntRange(5, 8);
  EXPECT_TRUE(range.contains(5));
  EXPECT_TRUE(range.contains(6));
  EXPECT_TRUE(range.contains(7));
  EXPECT_FALSE(range.contains(4));
  EXPECT_FALSE(range.contains(8));
}

TEST(range, First)
{
  IntRange range = IntRange(5, 8);
  EXPECT_EQ(range.first(), 5);
}

TEST(range, Last)
{
  IntRange range = IntRange(5, 8);
  EXPECT_EQ(range.last(), 7);
}
