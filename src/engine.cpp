#include "../inc/engine.h"
#include "../inc/util.h"
#include "../inc/board.h"
#include "../inc/engineSettings.h"

#include <chrono>
#include <atomic>

ChessEngine::ChessEngine()
:
m_pBoard(nullptr),
m_pppMoveLists(nullptr),
m_searchValues({}),
m_plyOfLastIrreversableMove(0),
m_previousZobKeys()
{

}

ChessEngine::~ChessEngine()
{

}

void ChessEngine::Init(Board* pBoard)
{
    m_pBoard = pBoard;
    m_pppMoveLists = new Move** [MaxEngineDepth];

    for (uint32 i = 0; i < MaxEngineDepth; i++)
    {
        m_pppMoveLists[i] = new Move* [MoveTypes::MoveTypeCount];

        m_pppMoveLists[i][MoveTypes::Best]   = new Move [NumBestMoves + 1];
        static_assert(NumBestMoves == 1);
        m_pppMoveLists[i][MoveTypes::Best][0].fromPiece = Piece::EndOfMoveList;
        m_pppMoveLists[i][MoveTypes::Best][1].fromPiece = Piece::EndOfMoveList;

        m_pppMoveLists[i][MoveTypes::ProbablyGood] = new Move[MaxNumProbablyGoodMoves + 1];
        for (uint32 j = 0; j < MaxNumProbablyGoodMoves + 1; j++)
        {
            m_pppMoveLists[i][MoveTypes::ProbablyGood][j].fromPiece = Piece::EndOfMoveList;
        }

        m_pppMoveLists[i][MoveTypes::Killer] = new Move [NumKillerMoves + 1];
        static_assert(NumKillerMoves == 2);
        m_pppMoveLists[i][MoveTypes::Killer][0].fromPiece = Piece::EndOfMoveList;
        m_pppMoveLists[i][MoveTypes::Killer][1].fromPiece = Piece::EndOfMoveList;
        m_pppMoveLists[i][MoveTypes::Killer][2].fromPiece = Piece::EndOfMoveList;

        m_pppMoveLists[i][MoveTypes::Attack] = new Move [MaxMovesPerPosition];
        m_pppMoveLists[i][MoveTypes::Normal] = new Move [MaxMovesPerPosition];
        for (uint32 j = 0; j < MaxMovesPerPosition; j++)
        {
            m_pppMoveLists[i][Attack][j].fromPiece = Piece::EndOfMoveList;
            m_pppMoveLists[i][Normal][j].fromPiece = Piece::EndOfMoveList;
        }
    }
    m_mainSearchTransTable.Init(MainTransTableSize);
    m_qSearchTransTable.Init(QSearchTransTableSize);
}

void ChessEngine::Destroy()
{
    for (uint32 i = 0; i < MaxEngineDepth; i++)
    {
        for (uint32 j = 0; j < MoveTypes::MoveTypeCount; j++)
        {
            delete[] m_pppMoveLists[i][j];
        }
        delete[] m_pppMoveLists[i];
    }
    delete[] m_pppMoveLists;

    m_pppMoveLists = nullptr;
    m_mainSearchTransTable.Destroy();
    m_qSearchTransTable.Destroy();
}

Move ChessEngine::DoEngine(EngineSettings     settings,
                           std::atomic<bool>& isTimedOut,
                           uint32*            pMaxDepth,
                           bool*              pIsMoveLegal)
{
    m_searchValues = {};

    Move bestMove = {};
    auto startTime = std::chrono::steady_clock::now();
    bool isMoveLegal = true;

    // Make a null move to trade turns to keep the board state consistent
    if (settings.isWhite)
    {
        if (m_pBoard->GetBoardStateIsWhiteTurn() == false)
        {
            std::cout << "Making null move to switch team (black->white)" << std::endl;
            m_pBoard->MakeNullMove<false>();
        }
    }
    else
    {
        if (m_pBoard->GetBoardStateIsWhiteTurn() == true)
        {
            std::cout << "Making null move to switch team (white->black)" << std::endl;
            m_pBoard->MakeNullMove<true>();
        }
    }

    if (settings.isWhite)
    {
        bestMove = IterativeDeepening<true>(settings.depth,
                                            settings.time,
                                            settings.useTime,
                                            settings.searchSettings,
                                            isTimedOut,
                                            pMaxDepth);
        
        isMoveLegal = m_pBoard->IsMoveLegal<true, true>(bestMove);
        if (settings.doMove && isMoveLegal)
        {
            m_pBoard->MakeMove<true>(bestMove);
        }
    }
    else
    {
        bestMove = IterativeDeepening<false>(settings.depth,
                                             settings.time,
                                             settings.useTime,
                                             settings.searchSettings,
                                             isTimedOut,
                                             pMaxDepth);

        isMoveLegal = m_pBoard->IsMoveLegal<false, true>(bestMove);
        if (settings.doMove && isMoveLegal)
        {
            m_pBoard->MakeMove<false>(bestMove);
        }
        // Need to invert the score since black will return the hightest value because of negmax,
        // however outside negmax, negative score is better for black.
        bestMove.score *= -1;
    }

    if ((pIsMoveLegal != nullptr) && (bestMove.fromPos != 0ull))
    {
        *pIsMoveLegal = isMoveLegal;
    }

    if (settings.printStats)
    {
        auto endTime = std::chrono::steady_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        uint32 knps = 0;
        if (totalTime.count() > 0)
        {
            knps = m_searchValues.positionsSearched / totalTime.count();
        }

        std::string bestMoveStr = m_pBoard->GetStringFromMove(bestMove);
        std::string scoreStr = ConvertScoreToStr(bestMove.score);

        std::cout << "Best Move          : " << bestMoveStr << std::endl;
        std::cout << "Score              : " << scoreStr << std::endl;

        std::cout << "Time               : " << totalTime.count() << " ms" << std::endl;
        std::cout << "Positions searched : " << m_searchValues.positionsSearched << std::endl;
        std::cout << "Knps               : " << knps << std::endl;

        std::cout << "Normal Searched       : " << m_searchValues.normalSearched << std::endl;
        std::cout << "Quiscence searched    : " << m_searchValues.quiscenceSearched << std::endl;
        std::cout << "TransTable hits       : " << m_searchValues.mainTransTableHits << std::endl;
        std::cout << "QSearch TT hits       : " << m_searchValues.qTransTableHits << std::endl;
        std::cout << "Null Move Prunes      : " << m_searchValues.nullMoveCutoffs << std::endl;
        std::cout << "Futility Prunes       : " << m_searchValues.futilityCutoffs << std::endl;
        std::cout << "Extended Fut. Prunes  : " << m_searchValues.extendedFutilityCutoffs << std::endl;
        std::cout << "MultiCut Prunes       : " << m_searchValues.multiCutCutoffs     << std::endl;
        std::cout << "Late Move Reductions  : " << m_searchValues.lateMoveReductions  << std::endl;
        std::cout << "Null Window ReSearches: " << m_searchValues.nullWindowReSearches << std::endl;
        std::cout << "Num Killer Moves Done : " << m_searchValues.numKillerMoves      << std::endl;
        std::cout << "NumDraws              : " << m_searchValues.drawsDetected       << std::endl;

        std::cout << std::flush;
    }

    return bestMove;
}

void ChessEngine::DoPerft(uint32 depth, bool isWhite, bool expanded)
{
    m_searchValues.positionsSearched = 0ull;
    auto startTime = std::chrono::steady_clock::now();

    if (expanded)
    {
        if (isWhite)
        {
            PerftExpanded<true>(depth);
        }
        else
        {
            PerftExpanded<false>(depth);
        }
    }
    else
    {
        if (isWhite)
        {
            Perft<true>(depth, 0);
        }
        else
        {
            Perft<false>(depth, 0);
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    uint32 knps = 0;
    if (totalTime.count() > 0)
    {
        knps = m_searchValues.positionsSearched / totalTime.count();
    }

    std::cout << "Time              : " << totalTime.count() << " ms" << std::endl;
    std::cout << "Positions searched: " << m_searchValues.positionsSearched << std::endl;
    std::cout << "Knps              : " << knps << std::endl;
}

template<bool isWhite>
uint32 ChessEngine::Perft(uint32 depth, uint32 ply)
{
    Move** ppMoveList = m_pppMoveLists[ply];

    uint32 numMoves = 0;

    m_pBoard->InvalidateCheckPinAndIllegalMoves();
    m_pBoard->GenerateLegalMoves<isWhite, false>(ppMoveList, &numMoves);

    // We've generated the moves all the moves for the last layer already, no reason to actually count them.
    if (depth <= 1)
    {
        m_searchValues.positionsSearched += numMoves;
        return 0;
    }


    GetNextMoveData nextMoveData = InitGetNextMoveData();
    const SearchSettings settings = {};
    Move            curMove      = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);

    BoardInfo prevBoardData = {};
    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyBoardData(&prevBoardData);
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));

    while (curMove.fromPiece != Piece::EndOfMoveList)
    {
        m_pBoard->MakeMove<isWhite>(curMove);
        Perft<!isWhite>(depth-1, ply+1);
        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);
    }

    return 0;
}

template<bool isWhite>
void ChessEngine::PerftExpanded(uint32 depth)
{
    Move** ppMoveList = m_pppMoveLists[0];
    m_pBoard->InvalidateCheckPinAndIllegalMoves();
    m_pBoard->GenerateLegalMoves<isWhite, false>(ppMoveList);

    BoardInfo prevBoardData = {};
    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyBoardData(&prevBoardData);
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));

    uint32 prevNumMoves = 0;
    std::string prevMoveStr = "00";

    GetNextMoveData nextMoveData = InitGetNextMoveData();
    const SearchSettings settings = {};
    Move            curMove      = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);

    while (curMove.fromPiece != Piece::EndOfMoveList)
    {
        m_pBoard->MakeMove<isWhite>(curMove);
        if (depth > 1)
        {
            Perft<!isWhite>(depth - 1, 1);
        }
        else
        {
            m_searchValues.positionsSearched++;
        }
        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        std::string moveStr = m_pBoard->GetStringFromMove(curMove);

        // print newline after each piece
        if ((prevMoveStr[0] != moveStr[0]) || (prevMoveStr[1] != moveStr[1]))
        {
            std::cout << std::endl;
        }
        prevMoveStr = moveStr;
        std::cout << moveStr << ": " << m_searchValues.positionsSearched - prevNumMoves << std::endl;
        prevNumMoves = m_searchValues.positionsSearched;

        curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);
    }
}

void ChessEngine::ResetPerftStats()
{
    m_searchValues.positionsSearched = 0;
}

template uint32 ChessEngine::Perft<true>(uint32 depth, uint32 ply);
template uint32 ChessEngine::Perft<false>(uint32 depth, uint32 ply);
template void ChessEngine::PerftExpanded<true>(uint32 depth);
template void ChessEngine::PerftExpanded<false>(uint32 depth);

template<bool isWhite>
Move ChessEngine::IterativeDeepening(
    uint32             depth,
    TimeType           searchTime,
    bool               useTime,
    SearchSettings     settings,
    std::atomic<bool>& isTimedOut, 
    uint32*            pMaxDepth)
{
    int32 alpha    = InitialAlpha;
    int32 beta     = InitialBeta;
    Move  bestMove = {};
    int32 score    = 0;

    // Assume last layer will take 30% of total time.  Should be 'good enough' for now
    TimeType maxTime = (searchTime * 7) / 10;

    auto startTime = std::chrono::steady_clock::now();
    uint32 searchDepth = 1;

    bool continueSearch = true;

    Move curMove = {};
    while (continueSearch)
    {
        if (settings.aspirationWindow)
        {
            alpha = score - settings.aspirationWindowSize;
            beta  = score + settings.aspirationWindowSize;
        }

        score = Negmax<isWhite, true>(searchDepth, 
                                      0,
                                      &curMove,
                                      alpha,
                                      beta,
                                      settings,
                                      isTimedOut);

        if ((settings.aspirationWindow) && ((score <= alpha) || (score >= beta)))
        {
            score = Negmax<isWhite, true>(searchDepth,
                                          0,
                                          &curMove,
                                          InitialAlpha,
                                          InitialBeta,
                                          settings,
                                          isTimedOut);
        }

        if (isTimedOut.load(std::memory_order_relaxed) == false)
        {
            bestMove = curMove;
        }

        searchDepth++;
        auto curTime = std::chrono::steady_clock::now();

        bool isCheckMate = (bestMove.score < NegCheckMateScore + 2*MaxEngineDepth) ||
                           (bestMove.score > PosCheckMateScore - 2*MaxEngineDepth);

        bool isStaleMate = (bestMove.score == 0)      &&
                           (bestMove.fromPos == 0ull) &&
                           (bestMove.toPos == 0ull);

        TimeType elapsedTime = std::chrono::duration_cast<TimeType>(curTime - startTime);
        continueSearch = (((searchDepth < depth)   && (useTime == false)) ||
                          ((elapsedTime < maxTime) && (useTime == true))) &&
                         (isTimedOut.load(std::memory_order_relaxed) == false) &&
                         (isCheckMate == false)                                &&
                         (isStaleMate == false);
    }

    if (pMaxDepth != nullptr)
    {
        *pMaxDepth = searchDepth - 1;
    }
    return bestMove;
}

template<bool isWhite, bool onPlyZero>
int32 ChessEngine::Negmax(
    int32              depth,
    uint32             ply,
    Move*              pBestMove,
    int32              alpha,
    int32              beta,
    SearchSettings     settings,
    std::atomic<bool>& isTimedOut)
{
    if (isTimedOut.load(std::memory_order_relaxed) && (onPlyZero == false))
    {
        return 0;
    }

    m_pBoard->InvalidateCheckPinAndIllegalMoves();
    m_searchValues.positionsSearched++;

    // We flip the team here because in reality we're checking whether or not the previous move
    // caused a draw by repetition.
    bool isDraw = m_pBoard->IsDrawByRepetition<!isWhite>();
    if (isDraw)
    {
        m_searchValues.drawsDetected++;
        if constexpr (onPlyZero)
        {
            *pBestMove = {};
            pBestMove->fromPos = 0ull;
            pBestMove->toPos   = 0ull;

            pBestMove->score   = 0;
        }
        return 0;
    }

    if constexpr (onPlyZero)
    {
        pBestMove->score = NegCheckMateScore;
    }

    if ((depth <= 0) || (ply >= MaxEngineDepth))
    {
        int32 score = QuiscenceSearch<isWhite>(ply, alpha, beta, settings, isTimedOut);
        return score;
    }
    m_searchValues.normalSearched++;

    // Prefetch the TT before generating the check and pin masks.  Prefetching the TT data make
    // the engine ~10% faster.
    uint64 zobKey = m_pBoard->GetZobKey();
    m_mainSearchTransTable.PrefetchEntry(zobKey);

    settings.expectedCutNode = !settings.expectedCutNode;
    if (settings.onPv)
    {
        settings.expectedCutNode = false;
    }

    // This will allow us to know if we are in check ahead of time
    m_pBoard->GenerateCheckAndPinMask<isWhite>();

    bool inCheck = m_pBoard->InCheck();

    TTScoreType ttScoreType = TTScoreType::LowerBound;
    bool ttMoveValid = false;
    Move ttMove = {};
    ttMove = m_mainSearchTransTable.ProbeTable(zobKey, depth, alpha, beta);

    ttMoveValid = ttMove.score != TTScoreNotFound;
    if (ttMoveValid)
    {
        ttMoveValid = m_pBoard->IsMoveLegal<isWhite>(ttMove);
    }
    // TT table hit.
    if ((ttMove.score != InvalidScore) && ttMoveValid)
    {
        if constexpr (onPlyZero)
        {
            *pBestMove = ttMove;
        }
        m_searchValues.mainTransTableHits++;
        return ttMove.score;
    }

    const bool canFutilityPrune = (settings.futilityPrune) &&
                                  (settings.onPv == false) &&
                                  (depth == 1) &&
                                  (inCheck == false);
    if (canFutilityPrune)
    {
        int32 futilityScore = m_pBoard->ScoreBoard<isWhite>();
        if (futilityScore < alpha - settings.futilityCutoff)
        {
            int32 futilityScore = QuiscenceSearch<isWhite>(ply, alpha, beta, settings, isTimedOut);
            m_searchValues.futilityCutoffs++;
            return futilityScore;
        }
    }

    const bool canExtendedFutilityPrune = (settings.extendedFutilityPrune) &&
                                          (settings.onPv == false)         &&
                                          (depth == 2)                     &&
                                          (inCheck == false);
    if (canExtendedFutilityPrune)
    {
        int32 futilityScore = m_pBoard->ScoreBoard<isWhite>();
        if (futilityScore < alpha - settings.extendedFutilityCutoff)
        {
            int32 futilityScore = QuiscenceSearch<isWhite>(ply, alpha, beta, settings, isTimedOut);
            m_searchValues.extendedFutilityCutoffs++;
            return futilityScore;
        }
    }

    BoardInfo prevBoardData = {};
    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyBoardData(&prevBoardData);
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));

    // If we can do a NullMoveSearch
    const bool canNullPrune = (settings.nullMovePrune)             &&
                              (settings.onPv == false)             &&
                              (inCheck == false);

    if (canNullPrune)
    {
        int32 nullMoveScore = 0;
        settings.nullMovePrune = false;

        int32 nullMoveSearchDepth = depth - settings.nullMoveDepth;

        m_pBoard->MakeNullMove<isWhite>();
        nullMoveScore = Negmax<!isWhite, false>(nullMoveSearchDepth,
                                                ply + 1,
                                                nullptr,
                                                0 - beta,
                                                1 - beta,
                                                settings,
                                                isTimedOut);
        nullMoveScore *= -1;
        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        if (nullMoveScore >= beta)
        {
            m_searchValues.nullMoveCutoffs++;
            return beta;
        }

        settings.nullMovePrune = true;
    }

    Move** ppMoveList  = m_pppMoveLists[ply];

    m_pBoard->GenerateLegalMoves<isWhite, false>(ppMoveList);
    GetNextMoveData nextMoveData = InitGetNextMoveData();

    // Need to do this after GenerateLegalMoves, since it puts invalidPiece in the spot
    if (ttMoveValid)
    {
        ppMoveList[MoveTypes::Best][0] = ttMove;
    }

    const bool canDoMultiCut = (settings.onPv == false)             &&
                               (settings.multiCutPrune == true)     &&
                               (inCheck == false)                   &&
                               (settings.expectedCutNode == true);
    if (canDoMultiCut)
    {
        uint32 numBetaCutoffs  = 0;
        settings.multiCutPrune = false;
        uint32 numMovesDone    = 0;
        int32 multiCutDepth    = depth - settings.multiCutDepth;


        Move curMove           = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);

        while((curMove.fromPiece != Piece::EndOfMoveList) && (numMovesDone < settings.multiCutMoves))
        {
            numMovesDone++;
            m_pBoard->MakeMove<isWhite>(curMove);
            int32 multiCutMoveScore = Negmax<!isWhite, false>(multiCutDepth,
                                                              ply + 1,
                                                              nullptr,
                                                              0 - beta,
                                                              1 - beta,
                                                              settings,
                                                              isTimedOut);
            multiCutMoveScore *= -1;
            m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

            if (multiCutMoveScore >= beta)
            {
                numBetaCutoffs++;
                if (numBetaCutoffs >= settings.multiCutThreshold)
                {
                    m_searchValues.multiCutCutoffs++;
                    return beta;
                }
            }
            curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);
        }
        settings.multiCutPrune = true;
    }

    int32 bestScore = NegCheckMateScore + ply;
    Move  bestMove  = {};

    nextMoveData.moveIdx = 0;
    nextMoveData.moveType = MoveTypes::Best;
    Move      curMove  = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);

    const bool canDoLateMoveReduction = (settings.onPv == false)             &&
                                        (settings.lateMoveReduction == true) &&
                                        (inCheck == false);

    uint32 numMoves = 0;
    int32 searchDepth = depth - 1;

    int32 searchBeta = beta;
    bool doNullWindowSearch = false;
    while (curMove.fromPiece != Piece::EndOfMoveList)
    {
        numMoves++;
        if (canDoLateMoveReduction)
        {
            // If we haven't beaten alpha and are on the late moves.
            if (ttScoreType != TTScoreType::Exact)
            {
                if (numMoves > settings.numLateMovesDiv)
                {
                    searchDepth = depth / settings.lateMoveDiv;
                    m_searchValues.lateMoveReductions++;
                }
                else if (numMoves > settings.numLateMovesSub)
                {
                    searchDepth = depth - settings.lateMoveSub;
                    m_searchValues.lateMoveReductions++;
                }
                else
                {
                    searchDepth = depth - 1;
                }
            }
            else
            {
                searchDepth = depth - 1;
            }
        }

        m_pBoard->MakeMove<isWhite>(curMove);
        int32 moveScore = Negmax<!isWhite, false>(searchDepth, 
                                                  ply + 1,
                                                  nullptr,
                                                  searchBeta * -1,
                                                  alpha      * -1,
                                                  settings,
                                                  isTimedOut);
        // Flip the sign since this is negmax
        moveScore *= -1;
        if (doNullWindowSearch)
        {
            // we need to re-search the move with a full window
            const bool needReSearch  = (moveScore > alpha) &&
                                       (moveScore < beta)  &&
                                       (numMoves  > 0)     &&
                                       (searchDepth > 0);
            if (needReSearch)
            {
                moveScore = Negmax<!isWhite, false>(searchDepth,
                                                    ply + 1,
                                                    nullptr,
                                                    beta  * -1,
                                                    alpha * -1,
                                                    settings,
                                                    isTimedOut);
                // Flip the sign since this is negmax
                moveScore *= -1;
                m_searchValues.nullWindowReSearches++;
            }
        }
        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        if (bestScore < moveScore)
        {
            bestMove       = curMove;
            bestMove.score = moveScore;
            bestScore      = moveScore;
        }

        if (onPlyZero == false)
        {
            settings.onPv = false;
        }

        if (bestScore > alpha)
        {
            alpha = bestScore;
            ttScoreType = TTScoreType::Exact;
            // wait until alpha increase before doing null window searches.
            doNullWindowSearch = settings.nullWindowSearch;
        }

        if (alpha >= beta)
        {
            ttScoreType = TTScoreType::UpperBound;
            if (settings.useKillerMoves)
            {
                InsertKillerMove(curMove, ply);
            }
            break;
        }

        if (doNullWindowSearch)
        {
            searchBeta = alpha + 1;
        }

        curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);
    }

    // stalemate
    if ((numMoves == 0) && (inCheck == false))
    {
        // Set the from pos to be 0ull to make sure we know it is a stalemate in the engine.
        bestMove.fromPos = 0ull;
        bestMove.toPos   = 0ull;
        bestMove.score   = 0;
        bestScore        = 0;
        m_searchValues.drawsDetected++;
        if (0 > alpha)
        {
            ttScoreType  = TTScoreType::Exact;
        }
    }

    if constexpr (onPlyZero)
    {
        *pBestMove = bestMove;
    }

    m_mainSearchTransTable.InsertToTable(m_pBoard->GetZobKey(), depth, bestMove, ttScoreType);

    return bestScore;
}

template<bool isWhite>
int32 ChessEngine::QuiscenceSearch(
    uint32             ply,
    int32              alpha,
    int32              beta,
    SearchSettings     settings,
    std::atomic<bool>& isTimedOut)
{
    if (isTimedOut.load(std::memory_order_relaxed) == true)
    {
        return 0;
    }

    m_pBoard->InvalidateCheckPinAndIllegalMoves();
    m_searchValues.positionsSearched++;
    m_searchValues.quiscenceSearched++;

    int32 standPatScore = m_pBoard->ScoreBoard<isWhite>();

    if (ply >= MaxEngineDepth)
    {
        return standPatScore;
    }

    // No point in continuing if either of these are true.
    if (standPatScore >= beta)
    {
        return beta;
    }

    // futility/delta pruning
    if (standPatScore < (alpha - PieceScores::QueenScore))
    {
        return alpha;
    }

    m_qSearchTransTable.PrefetchEntry(m_pBoard->GetZobKey());
    m_pBoard->GenerateCheckAndPinMask<isWhite>();

    TTScoreType ttScoreType = TTScoreType::LowerBound;
    Move ttMove = m_qSearchTransTable.ProbeTable(m_pBoard->GetZobKey(), 0, alpha, beta);

    bool ttMoveValid = ttMove.score != TTScoreNotFound;
    if (ttMoveValid)
    {
        ttMoveValid = m_pBoard->IsMoveLegal<isWhite>(ttMove);
    }
    // TT table hit.
    if ((ttMove.score != InvalidScore) && ttMoveValid)
    {
        m_searchValues.qTransTableHits++;
        return ttMove.score;
    }

    Move** ppMoveList = m_pppMoveLists[ply];

    const bool inCheck = m_pBoard->InCheck();
    if (inCheck)
    {
        m_pBoard->GenerateLegalMoves<isWhite, false>(ppMoveList);
    }
    else
    {
        m_pBoard->GenerateLegalMoves<isWhite, true>(ppMoveList);
    }

    if (ttMoveValid && IsMoveGoodForQsearch(ttMove, inCheck))
    {
        ppMoveList[MoveTypes::Best][0] = ttMove;
    }

    BoardInfo prevBoardData = {};
    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyBoardData(&prevBoardData);
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));

    int32 bestScore = standPatScore;

    Move  bestMove = {};
    bestMove.score = bestScore;

    if (bestScore > alpha)
    {
        alpha = bestScore;
    }

    bool didMove = false;
    GetNextMoveData nextMoveData = InitGetNextMoveData();
    Move            curMove      = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);
    while (curMove.fromPiece != Piece::EndOfMoveList)
    {
        // Once we're just capturing pawns, break out.
        if (IsMoveGoodForQsearch(curMove, inCheck) == false)
        {
            break;
        }
        m_pBoard->MakeMove<isWhite>(curMove);
        didMove = true;
        int32 moveScore = QuiscenceSearch<!isWhite>(ply + 1, beta * -1, alpha * -1, settings, isTimedOut);
        // Flip the sign since this is negmax
        moveScore *= -1;

        bestScore = (bestScore < moveScore) ? moveScore : bestScore;

        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        if (bestScore > alpha)
        {
            ttScoreType = TTScoreType::Exact;
            bestMove = curMove;
            bestMove.score = bestScore;
            alpha = bestScore;
        }

        if (alpha >= beta)
        {
            ttScoreType = TTScoreType::UpperBound;
            break;
        }
        curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData, settings);
    }

    if (didMove)
    {
        m_qSearchTransTable.InsertToTable(m_pBoard->GetZobKey(), 0, bestMove, ttScoreType);
    }
    else
    {
        bestScore = 0ull;
    }
    return bestScore;
}

std::string ChessEngine::ConvertScoreToStr(int32 score, int32* pCheckMateDepth)
{
    std::string scoreStr = "";
    // detect checkmate
    if (score > (PosCheckMateScore - (int32)(2 * MaxEngineDepth)))
    {
        int32 mateDepth = (PosCheckMateScore - (score - 1)) / 2;
        scoreStr = "White has mate in ";
        scoreStr += std::to_string(mateDepth);

        if (pCheckMateDepth != nullptr)
        {
            *pCheckMateDepth = mateDepth;
        }
    }
    else if (score < (NegCheckMateScore + (int32)(2 * MaxEngineDepth)))
    {
        score *= -1;
        int32 mateDepth = (PosCheckMateScore - (score - 1)) / 2;
        scoreStr = "Black has mate in ";
        scoreStr += std::to_string(mateDepth);
        if (pCheckMateDepth != nullptr)
        {
            *pCheckMateDepth = mateDepth;
        }
    }
    else
    {
        scoreStr = std::to_string((static_cast<float>(score) / 
                                   static_cast<float>(PieceScores::PawnScore)));
        if (pCheckMateDepth != nullptr)
        {
            *pCheckMateDepth = NotCheckMate;
        }
    }

    return scoreStr;
}

int32 ScoreMoveMVVLVA(const Move& move)
{
    int32 score  = 0;
    if (move.toPiece != Piece::NoPiece)
    {
        uint32 attacker = (move.fromPiece % 6);
        uint32 victim   = (move.toPiece   % 6);

        score = MVVLVA_arr[attacker][victim];
    }
    return score;
}

void SwapMoves(Move* pMoveList, int32 idx1, int32 idx2)
{
    Move tempMove = pMoveList[idx1];
    pMoveList[idx1] = pMoveList[idx2];
    pMoveList[idx2] = tempMove;
}

// Sort them using a simple in place insertion sort.
void ChessEngine::SortMoves(Move* pMoveList, const SearchSettings& settings)
{
    uint32 i = 0;
    // First, see if we can capture the piece on the last square that was moved to
    while (pMoveList[i].fromPiece != Piece::EndOfMoveList)
    {
        uint32 maxIdx = i;
        uint32 j = i + 1;
        while (pMoveList[j].fromPiece != Piece::EndOfMoveList)
        {
            if (pMoveList[j].score > pMoveList[maxIdx].score)
            {
                maxIdx = j;
            }

            // If we are capturing the piece on the square the enemy most recently moved to, then
            // immediately mark it as the best attack
            if (settings.searchReCaptureFirst)
            {
                if (pMoveList[j].toPos == m_pBoard->GetLastPosCaptured())
                {
                    maxIdx = j;
                    break;
                }
            }
            j++;
        }

        if (maxIdx != i)
        {
            SwapMoves(pMoveList, i, maxIdx);
        }
        i++;
    }
}

bool ChessEngine::IsMoveGoodForQsearch(const Move& move, bool inCheck)
{
    // don't check pawn captures in Qsearch.
    bool isCaptureOfNonPawn = (move.toPiece != Piece::NoPiece) &&
                              (move.toPiece != Piece::wPawn)   &&
                              (move.toPiece != Piece::bPawn);
    bool isPromotion        = ((move.flags & MoveFlags::Promotion) != 0ull);
    return isCaptureOfNonPawn || inCheck || isPromotion;
}

void ChessEngine::InsertKillerMove(const Move& move, uint32 ply)
{
    if (move.fromPiece != Piece::NoPiece)
    {
        Move* killerMoves = m_pppMoveLists[ply][MoveTypes::Killer];
        Move currKiller = killerMoves[0];
        const bool movesEqual = (move.fromPiece == currKiller.fromPiece) &&
                                (move.fromPos   == currKiller.fromPos)   &&
                                (move.toPiece   == currKiller.toPiece)   &&
                                (move.toPos     == currKiller.toPos);
        if (movesEqual == false)
        {
            memcpy(&(killerMoves[1]), &(killerMoves[0]), sizeof(Move));
            memcpy(&(killerMoves[0]), &move, sizeof(Move));
        }
    }
}

// Gets the next move.  Should initialize pMoveIdx and pMoveType to 0 and Best outside this function.
// This will sort the moves as needed.
template<bool isWhite>
Move ChessEngine::GetNextMove(Move** ppMoveList, GetNextMoveData* pData, const SearchSettings& settings)
{
    Move curMove = ppMoveList[pData->moveType][pData->moveIdx];

    while (curMove.fromPiece == Piece::EndOfMoveList)
    {
        pData->moveIdx = 0;
        switch (pData->moveType)
        {
            case(MoveTypes::Best):
                // Sort moves as needed
                if (pData->sortedProbGood == false)
                {
                    SortMoves(&(ppMoveList[MoveTypes::ProbablyGood][0]), settings);
                }
                pData->moveType = MoveTypes::ProbablyGood;
                break;
            case(MoveTypes::ProbablyGood):
                // Sort moves as needed
                if (pData->sortedAttacks == false)
                {
                    SortMoves(&(ppMoveList[MoveTypes::Attack][0]), settings);
                }
                pData->moveType = MoveTypes::Attack;
                break;
            case(MoveTypes::Attack):
                pData->moveType = MoveTypes::Killer;
                break;
            case(MoveTypes::Killer):
                pData->moveType = MoveTypes::Normal;
                break;
            case(MoveTypes::Normal):
                pData->moveType = MoveTypes::MoveTypeCount;
                break;
            default:
                break;
        }
        if (pData->moveType != MoveTypes::MoveTypeCount)
        {
            curMove = ppMoveList[pData->moveType][pData->moveIdx];
        }
        else
        {
            break;
        }
    }
    pData->moveIdx++;

    // Check for move legality.  If the move isn't legal, then recursively call this function
    // until we get a legal move.
    if (pData->moveType == MoveTypes::Killer)
    {
        bool isLegal = m_pBoard->IsMoveLegal<isWhite>(curMove);
        if (isLegal == false)
        {
            curMove = GetNextMove<isWhite>(ppMoveList, pData, settings);
        }
    }

    if (pData->moveType == MoveTypes::Killer)
    {
        m_searchValues.numKillerMoves++;
    }

    return curMove;
}

GetNextMoveData InitGetNextMoveData()
{
    GetNextMoveData data = {};
    data.moveIdx        = 0;
    data.moveIdx        = MoveTypes::Best;
    data.sortedAttacks  = false;
    data.sortedProbGood = false;

    return data;
}