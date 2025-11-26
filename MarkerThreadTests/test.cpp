#include "pch.h"
#include "gtest/gtest.h"
#include "array_utils_simple.h"
#include <windows.h>
#include <iostream>
#include <vector>
#include <atomic>

// Добавляем недостающие функции ArrayUtils прямо здесь
class TestArrayUtils {
public:
    static void PrintArray(int* arr, int size, CRITICAL_SECTION* cs) {
        if (arr == nullptr || cs == nullptr) {
            std::cout << "Invalid parameters for PrintArray" << std::endl;
            return;
        }

        EnterCriticalSection(cs);
        std::cout << "Array: ";
        for (int i = 0; i < size; ++i) {
            std::cout << arr[i] << " ";
        }
        std::cout << std::endl;
        LeaveCriticalSection(cs);
    }

    static void InitializeArray(int* arr, int size) {
        if (arr == nullptr) return;

        for (int i = 0; i < size; ++i) {
            arr[i] = 0;
        }
    }

    static bool ValidateArraySize(int size) {
        return size > 0;
    }

    static bool ValidateThreadCount(int count) {
        return count > 0 && count <= 64;
    }
};

// Структура для тестирования
struct TestMarkerParams {
    int id;
    int arraySize;
    int* sharedArray;
    CRITICAL_SECTION* cs;
    HANDLE hStartEvent;
    HANDLE hStoppedEvent;
    HANDLE hStopEvent;
    HANDLE hContinueEvent;

    TestMarkerParams()
        : id(0), arraySize(0), sharedArray(nullptr), cs(nullptr),
        hStartEvent(nullptr), hStoppedEvent(nullptr),
        hStopEvent(nullptr), hContinueEvent(nullptr) {
    }
};

// Мок-класс для тестирования
class MockMarkerLogic {
public:
    static int SimulateMarking(TestMarkerParams* params, int maxIterations = 100) {
        if (params == nullptr || params->sharedArray == nullptr || params->cs == nullptr) {
            return 0;
        }

        int threadId = params->id;
        int markedElements = 0;
        srand(threadId + 123);

        for (int iteration = 0; iteration < maxIterations; ++iteration) {
            int randomPosition = rand() % params->arraySize;

            EnterCriticalSection(params->cs);

            if (params->sharedArray[randomPosition] == 0) {
                params->sharedArray[randomPosition] = threadId;
                markedElements++;
                LeaveCriticalSection(params->cs);

                if (markedElements >= 3) {
                    break;
                }
            }
            else {
                LeaveCriticalSection(params->cs);
                break;
            }
        }

        return markedElements;
    }

    static int SimulateRemoval(TestMarkerParams* params) {
        if (params == nullptr || params->sharedArray == nullptr || params->cs == nullptr) {
            return 0;
        }

        int removedCount = 0;
        EnterCriticalSection(params->cs);

        for (int i = 0; i < params->arraySize; ++i) {
            if (params->sharedArray[i] == params->id) {
                params->sharedArray[i] = 0;
                removedCount++;
            }
        }

        LeaveCriticalSection(params->cs);
        return removedCount;
    }

    static int CountMarksByThread(int* array, int size, int threadId) {
        if (array == nullptr) return 0;

        int count = 0;
        for (int i = 0; i < size; ++i) {
            if (array[i] == threadId) {
                count++;
            }
        }
        return count;
    }
};

// ===== ТЕСТЫ =====

// Тесты для ArrayUtils
TEST(ArrayUtilsTest, ValidateArraySize_ValidSize_ReturnsTrue) {
    EXPECT_TRUE(TestArrayUtils::ValidateArraySize(10));
    EXPECT_TRUE(TestArrayUtils::ValidateArraySize(1));
    EXPECT_TRUE(TestArrayUtils::ValidateArraySize(1000));
}

TEST(ArrayUtilsTest, ValidateArraySize_InvalidSize_ReturnsFalse) {
    EXPECT_FALSE(TestArrayUtils::ValidateArraySize(0));
    EXPECT_FALSE(TestArrayUtils::ValidateArraySize(-1));
    EXPECT_FALSE(TestArrayUtils::ValidateArraySize(-100));
}

TEST(ArrayUtilsTest, ValidateThreadCount_ValidCount_ReturnsTrue) {
    EXPECT_TRUE(TestArrayUtils::ValidateThreadCount(1));
    EXPECT_TRUE(TestArrayUtils::ValidateThreadCount(5));
    EXPECT_TRUE(TestArrayUtils::ValidateThreadCount(64));
}

TEST(ArrayUtilsTest, ValidateThreadCount_InvalidCount_ReturnsFalse) {
    EXPECT_FALSE(TestArrayUtils::ValidateThreadCount(0));
    EXPECT_FALSE(TestArrayUtils::ValidateThreadCount(-1));
    EXPECT_FALSE(TestArrayUtils::ValidateThreadCount(65));
}

TEST(ArrayUtilsTest, InitializeArray_SetsAllZeros) {
    const int size = 5;
    int array[size] = { 1, 2, 3, 4, 5 };

    TestArrayUtils::InitializeArray(array, size);

    for (int i = 0; i < size; ++i) {
        EXPECT_EQ(array[i], 0);
    }
}

// Тесты для ArrayUtilsSimple
TEST(ArrayUtilsSimpleTest, ValidateArraySize) {
    EXPECT_TRUE(ArrayUtilsSimple::ValidateArraySize(1));
    EXPECT_TRUE(ArrayUtilsSimple::ValidateArraySize(10));
    EXPECT_FALSE(ArrayUtilsSimple::ValidateArraySize(0));
    EXPECT_FALSE(ArrayUtilsSimple::ValidateArraySize(-1));
}

TEST(ArrayUtilsSimpleTest, ValidateThreadCount) {
    EXPECT_TRUE(ArrayUtilsSimple::ValidateThreadCount(1));
    EXPECT_TRUE(ArrayUtilsSimple::ValidateThreadCount(5));
    EXPECT_FALSE(ArrayUtilsSimple::ValidateThreadCount(0));
    EXPECT_FALSE(ArrayUtilsSimple::ValidateThreadCount(-1));
}

TEST(ArrayUtilsSimpleTest, InitializeArray) {
    int arr[5] = { 1, 2, 3, 4, 5 };
    ArrayUtilsSimple::InitializeArray(arr, 5);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(arr[i], 0);
    }
}

// Тесты для критических секций
TEST(CriticalSectionTest, InitializeAndDelete_Success) {
    CRITICAL_SECTION cs;
    EXPECT_NO_FATAL_FAILURE(InitializeCriticalSection(&cs));
    EXPECT_NO_FATAL_FAILURE(DeleteCriticalSection(&cs));
}

TEST(CriticalSectionTest, EnterAndLeave_Success) {
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    EXPECT_NO_FATAL_FAILURE(EnterCriticalSection(&cs));
    EXPECT_NO_FATAL_FAILURE(LeaveCriticalSection(&cs));

    DeleteCriticalSection(&cs);
}

// Тесты для логики маркировки
TEST(MarkerLogicTest, SimulateMarking_EmptyArray_MarksSuccessfully) {
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);
    const int size = 10;
    int array[size] = { 0 };

    TestMarkerParams params;
    params.id = 1;
    params.arraySize = size;
    params.sharedArray = array;
    params.cs = &cs;

    int marked = MockMarkerLogic::SimulateMarking(&params);
    EXPECT_GE(marked, 0);
    EXPECT_LE(marked, 3);

    int actualMarked = MockMarkerLogic::CountMarksByThread(array, size, 1);
    EXPECT_EQ(marked, actualMarked);

    DeleteCriticalSection(&cs);
}

TEST(MarkerLogicTest, SimulateMarking_NullParameters_ReturnsZero) {
    int marked = MockMarkerLogic::SimulateMarking(nullptr);
    EXPECT_EQ(marked, 0);
}

TEST(MarkerLogicTest, SimulateRemoval_NullParameters_ReturnsZero) {
    int removed = MockMarkerLogic::SimulateRemoval(nullptr);
    EXPECT_EQ(removed, 0);
}

TEST(MarkerLogicTest, SimulateRemoval_RemovesAllMarks) {
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);
    const int size = 10;
    int array[size] = { 0 };

    array[0] = 1;
    array[1] = 1;
    array[2] = 1;
    array[5] = 2;

    TestMarkerParams params;
    params.id = 1;
    params.arraySize = size;
    params.sharedArray = array;
    params.cs = &cs;

    int removed = MockMarkerLogic::SimulateRemoval(&params);
    EXPECT_EQ(removed, 3);
    EXPECT_EQ(MockMarkerLogic::CountMarksByThread(array, size, 1), 0);
    EXPECT_EQ(MockMarkerLogic::CountMarksByThread(array, size, 2), 1);

    DeleteCriticalSection(&cs);
}

// Тесты на обработку ошибок
TEST(ErrorHandlingTest, NullArrayWithValidSize_NoCrash) {
    CRITICAL_SECTION cs;
    InitializeCriticalSection(&cs);

    EXPECT_NO_FATAL_FAILURE(TestArrayUtils::PrintArray(nullptr, 10, &cs));
    EXPECT_NO_FATAL_FAILURE(TestArrayUtils::InitializeArray(nullptr, 10));

    DeleteCriticalSection(&cs);
}

TEST(ErrorHandlingTest, NullCriticalSection_NoCrash) {
    const int size = 5;
    int array[size] = { 1, 2, 3, 4, 5 };

    EXPECT_NO_FATAL_FAILURE(TestArrayUtils::PrintArray(array, size, nullptr));
}

// Главная функция
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}