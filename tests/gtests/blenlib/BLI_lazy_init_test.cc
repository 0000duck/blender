#include "testing/testing.h"
#include "BLI_lazy_init.hpp"

LAZY_INIT_NO_ARG(void *, void *, get_single_pointer)
{
	return std::malloc(42);
}

TEST(lazy_init, NoArg_ReturnSame)
{
	void *ptr1 = get_single_pointer();
	void *ptr2 = get_single_pointer();
	EXPECT_EQ(ptr1, ptr2);
}