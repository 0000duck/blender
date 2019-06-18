#include "testing/testing.h"
#include "BLI_small_set.hpp"

using IntSet = BLI::SmallSet<int>;

TEST(small_set, Defaultconstructor)
{
  IntSet set;
  EXPECT_EQ(set.size(), 0);
}

TEST(small_set, ContainsNotExistant)
{
  IntSet set;
  EXPECT_FALSE(set.contains(3));
}

TEST(small_set, ContainsExistant)
{
  IntSet set;
  EXPECT_FALSE(set.contains(5));
  set.add(5);
  EXPECT_TRUE(set.contains(5));
}

TEST(small_set, AddMany)
{
  IntSet set;
  for (int i = 0; i < 100; i++) {
    set.add(i);
  }

  for (int i = 50; i < 100; i++) {
    EXPECT_TRUE(set.contains(i));
  }
  for (int i = 100; i < 150; i++) {
    EXPECT_FALSE(set.contains(i));
  }
}

TEST(small_set, InitializerListConstructor)
{
  IntSet set = {4, 5, 6};
  EXPECT_EQ(set.size(), 3);
  EXPECT_TRUE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  EXPECT_TRUE(set.contains(6));
  EXPECT_FALSE(set.contains(2));
  EXPECT_FALSE(set.contains(3));
}

TEST(small_set, CopyConstructor)
{
  IntSet set = {3};
  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));

  IntSet set2 = set;
  set2.add(4);
  EXPECT_TRUE(set2.contains(3));
  EXPECT_TRUE(set2.contains(4));

  EXPECT_FALSE(set.contains(4));
}

TEST(small_set, MoveConstructor)
{
  IntSet set = {1, 2, 3};
  EXPECT_EQ(set.size(), 3);
  IntSet set2 = std::move(set);
  EXPECT_EQ(set.size(), 0);
  EXPECT_EQ(set2.size(), 3);
}

TEST(small_set, Pop)
{
  IntSet set = {4};
  EXPECT_EQ(set.size(), 1);
  EXPECT_EQ(set.pop(), 4);
  EXPECT_EQ(set.size(), 0);
}

TEST(small_set, Remove)
{
  IntSet set = {3, 4, 5};
  EXPECT_TRUE(set.contains(3));
  EXPECT_TRUE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove(4);
  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove(3);
  EXPECT_FALSE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove(5);
  EXPECT_FALSE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_FALSE(set.contains(5));
}

TEST(small_set, RemoveMany)
{
  IntSet set;
  for (uint i = 0; i < 1000; i++) {
    set.add(i);
  }
  for (uint i = 100; i < 1000; i++) {
    set.remove(i);
  }
  for (uint i = 900; i < 1000; i++) {
    set.add(i);
  }

  for (uint i = 0; i < 1000; i++) {
    if (i < 100 || i >= 900) {
      EXPECT_TRUE(set.contains(i));
    }
    else {
      EXPECT_FALSE(set.contains(i));
    }
  }
}

TEST(small_set, Intersects)
{
  IntSet a = {3, 4, 5, 6};
  IntSet b = {1, 2, 5};
  EXPECT_TRUE(IntSet::Intersects(a, b));
  EXPECT_FALSE(IntSet::Disjoint(a, b));
}

TEST(small_set, Disjoint)
{
  IntSet a = {5, 6, 7, 8};
  IntSet b = {2, 3, 4, 9};
  EXPECT_FALSE(IntSet::Intersects(a, b));
  EXPECT_TRUE(IntSet::Disjoint(a, b));
}

TEST(small_set, AddMultiple)
{
  IntSet a;
  a.add_multiple({5, 7});
  EXPECT_TRUE(a.contains(5));
  EXPECT_TRUE(a.contains(7));
  EXPECT_FALSE(a.contains(4));
  a.add_multiple({2, 4});
  EXPECT_TRUE(a.contains(4));
  EXPECT_TRUE(a.contains(2));
}

TEST(small_set, ToSmallVector)
{
  IntSet a = {5, 2, 8};
  BLI::SmallVector<int> vec = a.to_small_vector();
  EXPECT_EQ(vec.size(), 3);
  EXPECT_TRUE(vec.contains(5));
  EXPECT_TRUE(vec.contains(2));
  EXPECT_TRUE(vec.contains(8));
}
