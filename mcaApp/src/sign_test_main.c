#include <stdio.h>

int main()
{
    signed char c=-1;
    int i=1000;
    
    i += c;
    printf("c=%d, i=%d\n", c, i);
}
