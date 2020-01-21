#include <iostream>
#if defined ZSIM
#include "PIMProfZSimHooks.h"
#endif
#if defined SNIPER
#include "PIMProfSniperHooks.h"
#endif
#if defined PIMPROF
#include "PIMProfAnnotation.h"
#endif

#include <cassert>

using namespace std;

void print4();
void pthreads_exec();

#ifndef NUM1
#define NUM1 9
#endif
#ifndef NUM2
#define NUM2 9
#endif
#ifndef ITER1
#define ITER1 3
#endif
// #ifndef ITER2
// #define ITER2 1
// #endif

#define ABS(x) ((x) < 0 ? -(x) : (x))

double **matrix_alloc(int size)
{
    double **ptr = (double **)malloc(sizeof(double *) * size);
    int i;
    for (i = 0; i < size; i++)
    {
        ptr[i] = (double *)malloc(sizeof(double) * size);
    }
    return ptr;
}

void matrix_free(double **ptr, int size)
{
    int i;
    for (i = 0; i < size; i++)
        free(ptr[i]);
    free(ptr);
}

void matrix_print(double **ptr, int size)
{
    int i, j;
    for (i = 0; i < size; i++)
    {
        for (j = 0; j < size; j++)
            printf("%lf\t", ptr[i][j]);
        printf("\n");
    }
}

void transpose(double **arr, int size)
{
    int i, j;
    for (i = 0; i < size; i++)
    {
        for (j = 0; j < i; j++)
        {
            double temp = arr[i][j];
            arr[i][j] = arr[j][i];
            arr[j][i] = temp;
        }
    }
}

// void inverse(double **arr, int size)
// {
//     int p[size];
//     int i, j, k;
//     for (i = 0; i < size; i++)
//     {
//         p[i] = i;
//     }
//     // in-place LUP decomposition
//     double **lu = matrix_alloc(size);
//     for (i = 0; i < size; i++)
//     {
//         for (j = 0; j < size; j++)
//             lu[i][j] = arr[i][j];
//     }

//     for (k = 0; k < size; k++)
//     {
//         double maxabs = 0;
//         int maxidx = 0;
//         for (i = k; i < size; i++)
//         {
//             if (ABS(lu[i][k]) > maxabs)
//             {
//                 maxabs = ABS(lu[i][k]);
//                 maxidx = i;
//             }
//         }
//         if (maxabs == 0)
//         {
//             assert(0 && "singular matrix\n");
//         }
//         int temp = p[k];
//         p[k] = p[maxidx];
//         p[maxidx] = temp;
//         for (i = 0; i < size; i++)
//         {
//             double dtemp = lu[k][i];
//             lu[k][i] = lu[maxidx][i];
//             lu[maxidx][i] = dtemp;
//         }
//         for (i = k + 1; i < size; i++)
//         {
//             lu[i][k] /= lu[k][k];
//             for (j = k + 1; j < size; j++)
//             {
//                 lu[i][j] -= (lu[i][k] * lu[k][j]);
//             }
//         }
//     }

//     // LUP solve
//     double *b = (double *)malloc(size * sizeof(double));
//     for (i = 0; i < size; i++)
//     {
//         b[i] = 0;
//     }
//     b[0] = 1;
//     double *y = (double *)malloc(size * sizeof(double));
//     for (k = 0; k < size; k++)
//     {
//         if (k > 0)
//         {
//             b[k - 1] = 0;
//             b[k] = 1;
//         }
//         for (i = 0; i < size; i++)
//         {
//             y[i] = b[p[i]];
//             for (j = 0; j < i; j++)
//             {
//                 y[i] -= (lu[i][j] * y[j]);
//             }
//         }
//         for (i = size - 1; i >= 0; i--)
//         {
//             arr[i][k] = y[i];
//             for (j = i + 1; j < size; j++)
//             {
//                 arr[i][k] -= (lu[i][j] * arr[j][k]);
//             }
//             arr[i][k] /= lu[i][i];
//         }
//     }
//     free(y);
//     free(b);
//     matrix_free(lu, size);
// }

// double **matrix_mul(double **result, double **x, double **y, int size)
// {
//     int i, j, k;
//     for (i = 0; i < size; i++)
//     {
//         for (j = 0; j < size; j++)
//         {
//             result[i][j] = 0;
//             for (k = 0; k < size; k++)
//             {
//                 result[i][j] += x[i][k] * y[k][j];
//             }
//         }
//     }
//     return result;
// }

int main()
{
#if defined ZSIM || defined SNIPER
    PIMPROF_BEGIN_PROGRAM
#endif
#if defined PIMPROF
    PIMProfROIDecisionBegin();
#endif

    srand(0);
    double **arr = matrix_alloc(NUM1);
    double **c = matrix_alloc(NUM1);
    int i, j;
    for (i = 0; i < NUM1; i++)
    {
        for (j = 0; j < NUM1; j++)
        {
            arr[i][j] = rand() % 20 + 1;
        }
    }
    for (i = 0; i < NUM1; i++)
    {
        for (j = 0; j < NUM1; j++)
        {
            c[i][j] = rand() % 20 + 1;
        }
    }

#if defined ZSIM || defined SNIPER
    PIMPROF_BEGIN_REG_PARALLEL
#endif
#if defined PIMPROF
    PIMProfROIBegin();
#endif

    for (i = 0; i < ITER1; i++)
        transpose(arr, NUM2);

#if defined ZSIM || defined SNIPER
    PIMPROF_END_REG_PARALLEL
#endif
#if defined PIMPROF
    PIMProfROIEnd();
#endif
    // pthreads_exec();
#if defined ZSIM || defined SNIPER
    PIMPROF_END_PROGRAM
#endif
#if defined PIMPROF
    PIMProfROIDecisionEnd();
#endif
    return 0;
}
