#include "myStdlib.h"

int maxArray(int *a, int n)
{
    int max = a[0];
    for ( int i = 1; i < n; i++ ) {
        if( max < a[i] ) max = a[i];
    }
    return max;
}
float maxArray(float *a, int n)
{
    float max = a[0];
    for ( int i = 1; i < n; i++ ) {
        if( max < a[i] ) max = a[i];
    }
    return max;
}