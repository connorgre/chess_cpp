#pragma once

#include "../inc/board.h"
#include "../inc/util.h"
#include "../inc/transTable.h"

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

constexpr int32 InvalidScore           = -0x5FFF;
constexpr int32 TTScoreNotFound        = -0x5FF0;

constexpr uint32 MainTransTableSize    = 15485867;  // large-ish prime
constexpr uint32 QSearchTransTableSize = 999983; // 999983;

constexpr int32 CastleScore = -10;

struct SearchSettings
{
    bool   onPv;                    //> Principle Variation doesn't have any pruning
    bool   nullMovePrune;           //> Prune moves where the other team can't improve their score
                                    //  with a free move on a reduced depth search
    uint32 nullMoveDepth;           //> Depth reduction for null moves

    bool  aspirationWindow;         //> On iterative deepening, try search with smaller initial
    int32 aspirationWindowSize;     //  alpha/beta window, retry with full window if we end up
                                    //  outside window

    bool   futilityPrune;           //> Skip to qSearch when we are down by more than a 
                                    //  futilityCutoff on depth 1
    uint32 futilityCutoff;          //> Futility value to cutoff with (knight value)

    bool   extendedFutilityPrune;   //> Skip to qSearch when we are down by more than
                                    //  extendedFutilityCutoff on depth 2
    uint32 extendedFutilityCutoff;  //> Extended futility value to cutoff with (rook value)
};

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
    int32 Negmax(uint32 depth, uint32 ply, Move* bestMove, int32 alpha, int32 beta, SearchSettings searchSettings);

    template<bool isWhite>
    int32 QuiscenceSearch(uint32 ply, int32 alpha, int32 beta);

    void SetupInitialSearchSettings(SearchSettings *pSearchSettings);

    void ResetTransTable() { m_mainSearchTransTable.ResetTable(); m_qSearchTransTable.ResetTable(); }
private:
    
    template<bool isWhite>
    Move IterativeDeepening(uint32 depth, SearchSettings searchSettings);

    TranspositionTable m_mainSearchTransTable;
    TranspositionTable m_qSearchTransTable;
    Board*  m_pBoard;
    Move*** m_pppMoveLists;

    struct
    {
        uint64  positionsSearched;
        uint64  quiscenceSearched;
        uint64  mainTransTableHits;
        uint64  qTransTableHits;
        uint64  nullMoveCutoffs;
        uint64  normalSearched;
        uint64  futilityCutoffs;
        uint64  extendedFutilityCutoffs;
    } m_searchValues;

    bool IsMoveGoodForQsearch(const Move& move, bool inCheck);

    std::string ConvertScoreToStr(int32 score);
};

void SortMoves(Move* pMoveList, uint32 numMoves);
int32 ScoreMoveMVVLVA(const Move& move);