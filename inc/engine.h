#pragma once

#include "../inc/board.h"
#include "../inc/util.h"

// Most Valuble Victim Least Valuble Attacker Array.
// Array indexed by [attacker][attackee] (attacker is the row, attacked is the column).  A more
// negative number is better for this.  So for example arr[pawn][king] means a pawn capturing a king
// would have a value of -56, the best possible option.  Or for example, a pawn capturing a queen (-46)
// is going to be treated better than a queen capturing a queen (-42).
                                     //k   q    r    b    n    p 
constexpr int32 MVVLVA_arr[6][6] = { {-56, -46, -36, -26, -16, -6},   //p
                                     {-55, -45, -35, -25, -15, -5},   //n
                                     {-54, -44, -34, -24, -14, -4},   //b
                                     {-53, -43, -33, -23, -13, -3},   //r
                                     {-52, -42, -32, -22, -12, -2},   //q
                                     {-51, -41, -31, -21, -11, -1} }; //k

constexpr uint32 MaxEngineDepth = 64;
constexpr int32 PosCheckMateScore =  0x6FFF;
constexpr int32 NegCheckMateScore = -0x6FFF;
constexpr int32 InitialAlpha      = -0x7FFF;
constexpr int32 InitialBeta       =  0x7FFF;

constexpr int32 CastleScore = -10;

class ChessEngine
{
public:
    ChessEngine();
    ~ChessEngine();

    void Init(Board* pBoard);
    void Destroy();

    void DoEngine(uint32 depth, bool isWhite, bool doMove);

    void DoPerft(uint32 depth, bool isWhite, bool expanded);

    template<bool isWhite>
    uint32 Perft(uint32 depth, uint32 ply);

    template<bool isWhite>
    void PerftExpanded(uint32 depth);

    void ResetPerftStats();

    template<bool isWhite, bool onPlyZero>
    int32 Negmax(uint32 depth, uint32 ply, Move* bestMove, int32 alpha, int32 beta);

    template<bool isWhite>
    int32 QuiscenceSearch(uint32 ply, int32 alpha, int32 beta);

private:

    Board* m_pBoard;
    Move   m_moveLists[MaxEngineDepth][MoveTypes::MoveTypeCount][MaxMovesPerPosition];
    uint64 m_positionsSearched;
    uint64 m_quiscenceSearched;

    bool IsMoveGoodForQsearch(const Move& move, bool inCheck);

    std::string ConvertScoreToStr(int32 score);
};

void SortMoves(Move* pMoveList, uint32 numMoves);
int32 ScoreMoveMVVLVA(const Move& move);