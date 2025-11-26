#include <iostream>
#include <vector>
#include <windows.h>
#include <ctime>
#include <process.h>

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

void PrintArray(int* arr, int size, CRITICAL_SECTION* cs) {
    EnterCriticalSection(cs);
    for (int i = 0; i < size; ++i) {
        std::cout << arr[i] << " ";
    }
    std::cout << std::endl;
    LeaveCriticalSection(cs);
}

DWORD WINAPI MarkerThread(LPVOID lpParam) {
    MarkerParams* params = (MarkerParams*)lpParam;
    int threadId = params->id;
    int markedElements = 0;

    WaitForSingleObject(params->hStartEvent, INFINITE);

    srand(threadId);

    HANDLE waitHandles[2] = {
        params->hContinueEvent,
        params->hStopEvent
    };

    while (true) {
        bool conflictDetected = false;
        int conflictPosition = -1;

        while (!conflictDetected) {
            int randomPosition = rand() % params->arraySize;

            EnterCriticalSection(params->cs);

            if (params->sharedArray[randomPosition] == 0) {
                Sleep(5);
                params->sharedArray[randomPosition] = threadId;
                markedElements++;
                Sleep(5);
                LeaveCriticalSection(params->cs);
            }
            else {
                conflictDetected = true;
                conflictPosition = randomPosition;
                LeaveCriticalSection(params->cs);
            }
        }

        std::cout << "--- Thread " << threadId << " paused. Marked: " << markedElements
            << ". Conflict at position " << conflictPosition << "." << std::endl;

        SetEvent(params->hStoppedEvent);

        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            std::cout << "--- Thread " << threadId << " continuing." << std::endl;
        }
        else {
            std::cout << "--- Thread " << threadId << " finishing." << std::endl;
            break;
        }
    }

    EnterCriticalSection(params->cs);

    std::cout << "--- Thread " << threadId << " removing marks..." << std::endl;
    int removedCount = 0;
    for (int i = 0; i < params->arraySize; ++i) {
        if (params->sharedArray[i] == threadId) {
            params->sharedArray[i] = 0;
            removedCount++;
        }
    }
    std::cout << "--- Thread " << threadId << " removed " << removedCount << " marks." << std::endl;

    LeaveCriticalSection(params->cs);

    delete params;
    return 0;
}

int main() {
    setlocale(LC_ALL, "Russian");

    int arrayLength;
    std::cout << "Enter array size: ";
    std::cin >> arrayLength;
    if (arrayLength <= 0) {
        std::cout << "Invalid size." << std::endl;
        return 1;
    }
    int* mainArray = new int[arrayLength]();

    int threadCount;
    std::cout << "Enter number of marker threads: ";
    std::cin >> threadCount;
    if (threadCount <= 0 || threadCount > MAXDWORD) {
        std::cout << "Invalid thread count." << std::endl;
        delete[] mainArray;
        return 1;
    }

    CRITICAL_SECTION criticalSection;
    InitializeCriticalSection(&criticalSection);

    std::vector<HANDLE> threadHandles(threadCount);
    std::vector<HANDLE> stoppedSignals(threadCount);
    std::vector<HANDLE> terminationSignals(threadCount);
    std::vector<HANDLE> resumeSignals(threadCount);
    std::vector<bool> activeThreads(threadCount, true);
    int runningThreads = threadCount;

    HANDLE globalStartSignal = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        NULL
    );

    for (int i = 0; i < threadCount; ++i) {
        stoppedSignals[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        terminationSignals[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
        resumeSignals[i] = CreateEvent(NULL, FALSE, FALSE, NULL);

        MarkerParams* threadParams = new MarkerParams;
        threadParams->id = i + 1;
        threadParams->arraySize = arrayLength;
        threadParams->sharedArray = mainArray;
        threadParams->cs = &criticalSection;
        threadParams->hStartEvent = globalStartSignal;
        threadParams->hStoppedEvent = stoppedSignals[i];
        threadParams->hStopEvent = terminationSignals[i];
        threadParams->hContinueEvent = resumeSignals[i];

        threadHandles[i] = CreateThread(NULL, 0, MarkerThread, threadParams, 0, NULL);
        if (threadHandles[i] == NULL) {
            std::cerr << "Failed to create thread " << i + 1 << std::endl;
        }
    }

    std::cout << threadCount << " threads created. Starting..." << std::endl;

    SetEvent(globalStartSignal);

    while (runningThreads > 0) {
        std::vector<HANDLE> currentStoppedSignals;
        for (int i = 0; i < threadCount; ++i) {
            if (activeThreads[i]) {
                currentStoppedSignals.push_back(stoppedSignals[i]);
            }
        }

        if (currentStoppedSignals.empty()) {
            break;
        }

        std::cout << "\nMain: Waiting for " << runningThreads << " threads to pause..." << std::endl;
        WaitForMultipleObjects(
            (DWORD)currentStoppedSignals.size(),
            currentStoppedSignals.data(),
            TRUE,
            INFINITE
        );
        std::cout << "Main: All active threads paused." << std::endl;

        std::cout << "Main: Current array state:" << std::endl;
        PrintArray(mainArray, arrayLength, &criticalSection);

        int selectedThread = -1;
        bool validSelection = false;
        while (!validSelection) {
            std::cout << "Enter thread to terminate (active: ";
            for (int i = 0; i < threadCount; ++i) {
                if (activeThreads[i]) {
                    std::cout << i + 1 << " ";
                }
            }
            std::cout << "): ";
            std::cin >> selectedThread;

            if (selectedThread < 1 || selectedThread > threadCount) {
                std::cout << "Error: invalid thread number." << std::endl;
            }
            else if (!activeThreads[selectedThread - 1]) {
                std::cout << "Error: thread " << selectedThread << " already finished." << std::endl;
            }
            else {
                validSelection = true;
            }
        }

        int threadIndex = selectedThread - 1;

        std::cout << "Main: Sending termination signal to thread " << selectedThread << "." << std::endl;
        activeThreads[threadIndex] = false;
        SetEvent(terminationSignals[threadIndex]);

        WaitForSingleObject(threadHandles[threadIndex], INFINITE);
        std::cout << "Main: Thread " << selectedThread << " confirmed termination." << std::endl;

        CloseHandle(threadHandles[threadIndex]);
        CloseHandle(terminationSignals[threadIndex]);
        CloseHandle(stoppedSignals[threadIndex]);
        CloseHandle(resumeSignals[threadIndex]);

        runningThreads--;

        std::cout << "Main: Array after cleanup by thread " << selectedThread << ":" << std::endl;
        PrintArray(mainArray, arrayLength, &criticalSection);

        if (runningThreads > 0) {
            std::cout << "Main: Resuming " << runningThreads << " remaining threads." << std::endl;
            for (int i = 0; i < threadCount; ++i) {
                if (activeThreads[i]) {
                    SetEvent(resumeSignals[i]);
                }
            }
        }
    }

    std::cout << "\nAll marker threads finished." << std::endl;

    CloseHandle(globalStartSignal);
    DeleteCriticalSection(&criticalSection);
    delete[] mainArray;

    std::cout << "Resources freed. Press Enter to exit." << std::endl;
    std::cin.ignore();
    std::cin.get();

    return 0;
}