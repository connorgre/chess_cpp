#pragma once

#include "../inc/board.h"
#include "../inc/util.h"
#include <chrono>

constexpr uint32 MaxEngineDepth = 64;
constexpr int32 PosCheckMateScore =  0x6FFF;
constexpr int32 NegCheckMateScore = -0x6FFF;
constexpr int32 InitialAlpha      = -0x7FFF;
constexpr int32 InitialBeta       =  0x7FFF;

class ChessEngine
{
public:
    ChessEngine();
    ~ChessEngine();

    void Init(Board* pBoard);
    void Destroy();

    void DoEngine(uint32 depth, bool isWhite);

    void DoPerft(uint32 depth, bool isWhite, bool expanded);

    template<bool isWhite>
    uint32 Perft(uint32 depth, uint32 ply);

    template<bool isWhite>
    void PerftExpanded(uint32 depth);

    void ResetPerftStats();

    template<bool isWhite, bool onPlyZero>
    int32 Negmax(uint32 depth, uint32 ply, Move* bestMove, int32 alpha, int32 beta);

private:

    Board* m_pBoard;
    Move   m_moveLists[MaxEngineDepth][MaxMovesPerPosition];
    uint64 m_positionsSearched;

    std::string ConvertScoreToStr(int32 score);
};