#pragma once
#include "marker_utils.h"

class MarkerThreadLogic {
public:
    static DWORD WINAPI ThreadFunction(LPVOID lpParam);

private:
    static void MarkElements(MarkerParams* params);
    static void RemoveMarks(MarkerParams* params);
    static int FindConflictPosition(MarkerParams* params);
};