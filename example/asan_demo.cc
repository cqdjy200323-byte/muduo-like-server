#include <iostream>

int main()
{
    int *a = new int[3];
    a[3] = 1; // 数组只有 a[0]~a[2]
    delete[] a;
    return 0;
}