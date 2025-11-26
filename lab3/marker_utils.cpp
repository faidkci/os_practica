#include "marker_utils.h"
#include <iostream>

void ArrayUtils::PrintArray(int* arr, int size, CRITICAL_SECTION* cs) {
    if (arr == nullptr || cs == nullptr) {
        std::cout << "Invalid parameters for PrintArray" << std::endl;
        return;
    }

    EnterCriticalSection(cs);
    for (int i = 0; i < size; ++i) {
        std::cout << arr[i] << " ";
    }
    std::cout << std::endl;
    LeaveCriticalSection(cs);
}

void ArrayUtils::InitializeArray(int* arr, int size) {
    if (arr == nullptr) return;

    for (int i = 0; i < size; ++i) {
        arr[i] = 0;
    }
}

bool ArrayUtils::ValidateArraySize(int size) {
    return size > 0;
}

bool ArrayUtils::ValidateThreadCount(int count) {
    return count > 0 && count <= 64;
}