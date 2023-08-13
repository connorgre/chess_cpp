#pragma once

#include "../inc/board.h"
#include "../inc/util.h"
#include "../inc/transTable.h"
#include <chrono>
#include <atomic>

// Most Valuble Victim Least Valuble Attacker Array.
// Array indexed by [attacker][attackee] (attacker is the row, attacked is the column). So for 
// example arr[pawn][king] means a pawn capturing a king would have a value of -56, the best
// possible option.  Or for example, a pawn capturing a queen (-46) is going to be treated better
// than a queen capturing a queen (-42).
                                     //k   q   r   b   n  p
constexpr int32 MVVLVA_arr[6][6] = { {41, 31, 21, 12, 11, 1},   //k
                                     {42, 32, 22, 13, 12, 2},   //q
                                     {43, 33, 23, 14, 13, 3},   //r
                                     {44, 34, 24, 15, 14, 4},   //b
                                     {45, 35, 25, 16, 15, 5},   //n
                                     {46, 36, 26, 17, 16, 6} }; //p

constexpr int32 MaxEngineDepth = 64;
constexpr int32 PosCheckMateScore =  0x6FFF;
constexpr int32 NegCheckMateScore = -0x6FFF;
constexpr int32 InitialAlpha      = -0x7FFF;
constexpr int32 InitialBeta       =  0x7FFF;

constexpr int32 InvalidScore           = -0x5FFF;
constexpr int32 TTScoreNotFound        = -0x5FF0;

constexpr uint32 MainTransTableSize    = 8000009; //15485867;  // large-ish prime
constexpr uint32 QSearchTransTableSize =  999983; // 999983;

constexpr int32 CastleScore = 150;

constexpr uint32 NumBestMoves   = 1;
constexpr uint32 NumKillerMoves = 2;

constexpr int32 NotCheckMate = -999;

struct GetNextMoveData
{
    uint32    moveIdx;
    MoveTypes moveType;
    bool      sortedAttacks;
    bool      sortedProbGood;
};

struct SearchSettings
{
    bool   onPv;                    //> Principle Variation doesn't have any pruning

    bool   expectedCutNode;         //> If we expect a beta cutoff (ie should we do multi-cut)

    bool   useKillerMoves;          //> Keep track of non captures that cause beta cutoff on same
                                    //  ply

    bool   nullWindowSearch;        //> Search with an alpha beta window of 1, research if we get a
                                    //  cutoff

    bool   nullMovePrune;           //> Prune moves where the other team can't improve their score
                                    //  with a free move on a reduced depth search
    uint32 nullMoveDepth;           //> Depth reduction for null moves

    bool  aspirationWindow;         //> On iterative deepening, try search with smaller initial
    int32 aspirationWindowSize;     //  alpha/beta window, retry with full window if we end up
                                    //  outside window

    bool   futilityPrune;           //> Skip to qSearch when we are down by more than a 
                                    //  futilityCutoff on depth 1
    int32 futilityCutoff;          //> Futility value to cutoff with (knight value)

    bool   extendedFutilityPrune;   //> Skip to qSearch when we are down by more than
                                    //  extendedFutilityCutoff on depth 2
    int32 extendedFutilityCutoff;  //> Extended futility value to cutoff with (rook value)

    bool   multiCutPrune;           //> Do a reduced depth search on the first multiCutMoves, if
    int32 multiCutMoves;           //  the number of beta cutoffs >= multiCutThreshold, then
    int32 multiCutThreshold;       //  return beta
    int32 multiCutDepth;

    bool   lateMoveReduction;       //> If none of the first numLateMovesSub beat alpha, then
    int32 numLateMovesSub;         //  subtract lateMoveSub from depth.  If none of the first
    int32 numLateMovesDiv;         //  numLateMovesDiv beat alpha, then divide depth by
    int32 lateMoveSub;             //  lateMoveDiv
    int32 lateMoveDiv;

    bool   searchReCaptureFirst;    //> Always put re-captures as the best move

    bool   useCounterMoveTable;     //> Order normal using the countermove table.

    bool   doCheckExtension;        //> Increase depth by 1 if we're in check.

    int32 quiescenceDepthLimit;    //> limit the depth quiescence search can go to.

    bool   doDeltaPruning;          //> don't bother continuing qsearch if move doesn't improve our
    int32 deltaPruningVal;         //  score by at least deltaPruningVal

    bool  doNullMoveReduction;      //> This is different than null move pruning.  What this does
    int32 nullReductionDepth;       //  is decrease the rest of the search depth if a very reduced
    int32 nullReductionSearchDepth; //  depth null move search leads to a beta cutoff.
};

typedef std::chrono::milliseconds TimeType;
struct EngineSettings
{
    uint32         depth;
    TimeType       time;
    bool           useTime;
    bool           isWhite;
    bool           doMove;
    bool           printStats;
    SearchSettings searchSettings;
};

class ChessEngine
{
public:
    ChessEngine();
    ~ChessEngine();

    void Init(Board* pBoard);
    void Destroy();

    Move DoEngine(EngineSettings     settings,
                  std::atomic<bool>& isTimedOut,
                  uint32*            pMaxDepth = nullptr,
                  bool*              pIsMoveLegal = nullptr);

    void DoPerft(uint32 depth, bool isWhite, bool expanded);

    template<bool isWhite>
    uint32 Perft(uint32 depth, uint32 ply);

    template<bool isWhite>
    void PerftExpanded(uint32 depth);

    void ResetPerftStats();

    template<bool isWhite, bool onPlyZero>
    int32 Negmax(int32              depth,
                 int32              ply,
                 Move*              bestMove,
                 int32              alpha,
                 int32              beta,
                 SearchSettings     searchSettings,
                 std::atomic<bool>& isTimedOut);

    template<bool isWhite>
    int32 QuiscenceSearch(int32              ply, 
                          int32              alpha,
                          int32              beta,
                          SearchSettings     settings,
                          std::atomic<bool>& isTimedOut,
                          int32              maxFreePly,
                          uint64             movedPieces=0ull);

    void ResetTransTable() { m_engineMainTTs[0].ResetTable(); m_engineMainTTs[1].ResetTable();
                          m_engineQSearchTTs[0].ResetTable(); m_engineQSearchTTs[1].ResetTable(); }

    std::string ConvertScoreToStr(int32 score, int32* pCheckMateDepth = nullptr);

    void ResetKillers();

private:
    template<bool isWhite>
    Move IterativeDeepening(
        uint32              depth, 
        TimeType            searchTime, 
        bool                useTime, 
        SearchSettings      searchSettings,
        std::atomic<bool>&  isTimedOut,
        uint32*             pMaxDepth = nullptr);

    void InsertKillerMove(const Move& move, uint32 ply);
    void InsertCounterMove(const Move& move);

    TranspositionTable m_engineMainTTs[2];
    TranspositionTable m_engineQSearchTTs[2];

    TranspositionTable* m_pMainSearchTransTable;
    TranspositionTable* m_pQSearchTransTable;

    Board*  m_pBoard;
    Move*** m_pppMoveLists;

    // stores the refutation to the previous move
    Move    m_counterMoveTable[Piece::PieceCount][64];

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
        uint64  multiCutCutoffs;
        uint64  lateMoveReductions;
        uint64  nullWindowReSearches;
        uint64  numKillerMoves;
        uint64  drawsDetected;
        uint64  killersIllegal;
        uint64  numNullReductions;
    } m_searchValues;

    bool IsMoveGoodForQsearch(
        const Move&           move,
        const SearchSettings& settings,
        bool                  inCheck,
        int32                 standPat,
        int32                 alpha,
        uint32                ply,
        uint32                maxFreePly,
        uint64                movedPieces);

    template<bool isWhite>
    Move GetNextMove(Move** ppMoveList, GetNextMoveData* pData, const SearchSettings& settings);

    void SortMoves(Move* pMoveList, const SearchSettings& settings);

};


int32 ScoreMoveMVVLVA(const Move& move);
GetNextMoveData InitGetNextMoveData();