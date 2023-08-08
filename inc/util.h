#pragma once

#include <iostream>
#include <iomanip>

#define VERIFY_BOARD _DEBUG

static void ChDebugAssert(
    bool  expr,
    char* exprStr,
    bool  isAssert,
    char* file, 
    int   line)
{
    if (expr == false)
    {
        const char assertStr[] = "Assert: ";
        const char alertStr[]  = "Alert:  ";
        const char* alertOrAssertStr = (isAssert) ? assertStr : alertStr;
        std::cout << alertOrAssertStr << std::setw(24) << std::left;
        std::cout << exprStr << " -- failed -- ";
        std::cout << line << ": " << file << std::endl;
    }
}

static void ChPrintMessage(char* msg, char* file, int line)
{
    std::cout << "Message: " << msg << " -- " << line << ": " << file << std::endl;
}

#ifdef _DEBUG
#define CH_ASSERT(_expr) \
    { \
        ChDebugAssert(_expr, #_expr, true, __FILE__, __LINE__); \
        if (_expr == false) \
        { \
            __debugbreak(); \
        } \
    }
#define CH_ALERT(_expr)  \
    { ChDebugAssert(_expr, #_expr, false, __FILE__, __LINE__); }
#define CH_MESSAGE(_printMsg) \
    { ChPrintMessage(_printMsg, __FILE__, __LINE__); }
#else
#define CH_ASSERT(_expr)
#define CH_ALERT(_expr)
#define CH_MESSAGE(_msg)
#endif

typedef long                int32;
typedef long long           int64;

typedef unsigned long       uint32;
typedef unsigned long long  uint64;

typedef char                int8;
typedef unsigned char       uint8;

static constexpr uint64 FullBoard = UINT64_MAX;

enum class Result : int32
{
    Success             = 0,
    Error               = -1,
    ErrorNotImplemented = -2,
    ErrorInvalidInput   = -3,
    ErrorInvalidBoard   = -4,
};

enum Directions : uint32
{
    North = 0,
    East = 1,
    South = 2,
    West = 3,
    NorthEast = 4,
    NorthWest = 5,
    SouthEast = 6,
    SouthWest = 7,

    Count,
};

// Helpful for getting exact positions easier
enum PositionLookup : uint8
{
    A1 =  0, B1 =  1, C1 =  2, D1 =  3, E1 =  4, F1 =  5, G1 =  6, H1 =  7,
    A2 =  8, B2 =  9, C2 = 10, D2 = 11, E2 = 12, F2 = 13, G2 = 14, H2 = 15,
    A3 = 16, B3 = 17, C3 = 18, D3 = 19, E3 = 20, F3 = 21, G3 = 22, H3 = 23,
    A4 = 24, B4 = 25, C4 = 26, D4 = 27, E4 = 28, F4 = 29, G4 = 30, H4 = 31,
    A5 = 32, B5 = 33, C5 = 34, D5 = 35, E5 = 36, F5 = 37, G5 = 38, H5 = 39,
    A6 = 40, B6 = 41, C6 = 42, D6 = 43, E6 = 44, F6 = 45, G6 = 46, H6 = 47,
    A7 = 48, B7 = 49, C7 = 50, D7 = 51, E7 = 52, F7 = 53, G7 = 54, H7 = 55,
    A8 = 56, B8 = 57, C8 = 58, D8 = 59, E8 = 60, F8 = 61, G8 = 62, H8 = 63,
};

