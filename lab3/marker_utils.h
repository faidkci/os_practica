#pragma once
#include <windows.h>
#include <iostream>  // Äëÿ std::cout

struct MarkerParams {
    int id;
    int arraySize;
    int* sharedArray;
    CRITICAL_SECTION* cs;
    HANDLE hStartEvent;
    HANDLE hStoppedEvent;
    HANDLE hStopEvent;
    HANDLE hContinueEvent;
};

class ArrayUtils {
public:
    static void PrintArray(int* arr, int size, CRITICAL_SECTION* cs);
    static void InitializeArray(int* arr, int size);
    static bool ValidateArraySize(int size);
    static bool ValidateThreadCount(int count);
};