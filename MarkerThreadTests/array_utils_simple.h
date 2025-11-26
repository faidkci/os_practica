#pragma once
#include <windows.h>

class ArrayUtilsSimple {
public:
    static bool ValidateArraySize(int size) {
        return size > 0;
    }

    static bool ValidateThreadCount(int count) {
        return count > 0 && count <= 64;
    }

    static void InitializeArray(int* arr, int size) {
        if (arr == nullptr || size <= 0) return;
        for (int i = 0; i < size; ++i) {
            arr[i] = 0;
        }
    }
}; 