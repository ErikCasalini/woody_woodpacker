#include <stdio.h>

static int helper(void)
{
    return 42;
}

int api(void)
{
    return helper();
}

int main(void)
{
    printf("%d\n", api());
}
