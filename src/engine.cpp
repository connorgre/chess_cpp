#include "../inc/engine.h"
#include "../inc/util.h"
#include "../inc/board.h"
#include <chrono>

ChessEngine::ChessEngine()
:
m_pBoard(nullptr),
m_pppMoveLists(nullptr),
m_searchValues({})
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

void ChessEngine::SetupInitialSearchSettings(SearchSettings* pSettings)
{
    pSettings->onPv                   = true;
    pSettings->nullWindowSearch       = true;
    pSettings->useKillerMoves         = true;

    pSettings->nullMovePrune          = true;
    pSettings->nullMoveDepth          = 4;

    pSettings->aspirationWindow       = true;
    pSettings->aspirationWindowSize   = PieceScores::PawnScore;

    pSettings->futilityPrune          = true;
    pSettings->futilityCutoff         = PieceScores::KnightScore;

    pSettings->extendedFutilityPrune  = true;
    pSettings->extendedFutilityCutoff = PieceScores::RookScore;

    pSettings->multiCutPrune          = true;
    pSettings->multiCutMoves          = 6;
    pSettings->multiCutThreshold      = 3;
    pSettings->mulitCutDepth          = 3;

    pSettings->lateMoveReduction      = true;
    pSettings->numLateMovesSub        = 3;
    pSettings->numLateMovesDiv        = 6;
    pSettings->lateMoveSub            = 1;
    pSettings->lateMoveDiv            = 3;
}

void ChessEngine::DoEngine(uint32 depth, bool isWhite, bool doMove)
{
    m_searchValues = {};

    Move bestMove = {};
    auto startTime = std::chrono::steady_clock::now();

    SearchSettings settings = {};
    SetupInitialSearchSettings(&settings);

    if (isWhite)
    {
        bestMove = IterativeDeepening<true>(depth, settings);
        if (doMove)
        {
            m_pBoard->MakeMove<true>(bestMove);
        }
    }
    else
    {
        bestMove = IterativeDeepening<false>(depth, settings);
        if (doMove)
        {
            m_pBoard->MakeMove<false>(bestMove);
        }
        // Need to invert the score since black will return the hightest value because of negmax,
        // however outside negmax, negative score is better for black.
        bestMove.score *= -1;
    }

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
    std::cout << "MultiCut Prunes       : " << m_searchValues.multiCutCutoffs << std::endl;
    std::cout << "Late Move Reductions  : " << m_searchValues.lateMoveReductions << std::endl;
    std::cout << "Null Window ReSearches: " << m_searchValues.nullWindowReSearches << std::endl;
    std::cout << "Num Killer Moves Done : " << m_searchValues.numKillerMoves << std::endl;
    std::cout << std::flush;
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
    Move            curMove      = GetNextMove<isWhite>(ppMoveList, &nextMoveData);

    BoardInfo prevBoardData = {};
    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyBoardData(&prevBoardData);
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));

    while (curMove.fromPiece != Piece::EndOfMoveList)
    {
        m_pBoard->MakeMove<isWhite>(curMove);
        Perft<!isWhite>(depth-1, ply+1);
        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData);
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
    Move            curMove      = GetNextMove<isWhite>(ppMoveList, &nextMoveData);

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

        curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData);
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
Move ChessEngine::IterativeDeepening(uint32 depth, SearchSettings settings)
{
    int32 alpha = InitialAlpha;
    int32 beta  = InitialBeta;
    Move  bestMove = {};

    uint32 initialDepth = (depth < 4) ? depth : 4;
    int32 score = Negmax<isWhite, true>(initialDepth, 0, &bestMove, alpha, beta, settings);

    for (uint32 searchDepth = initialDepth + 1; searchDepth < depth; searchDepth++)
    {
        if (settings.aspirationWindow)
        {
            alpha = score - settings.aspirationWindowSize;
            beta  = score + settings.aspirationWindowSize;
        }

        score = Negmax<isWhite, true>(searchDepth, 0, &bestMove, alpha, beta, settings);

        if ((settings.aspirationWindow) && ((score <= alpha) || (score >= beta)))
        {
            std::cout << "did full window search" << std::endl;
            score = Negmax<isWhite, true>(searchDepth, 0, &bestMove, InitialAlpha, InitialBeta, settings);
        }
    }
    return bestMove;
}

template<bool isWhite, bool onPlyZero>
int32 ChessEngine::Negmax(
    uint32         depth, 
    uint32         ply, 
    Move*          pBestMove, 
    int32          alpha, 
    int32          beta, 
    SearchSettings settings)
{
    m_pBoard->InvalidateCheckPinAndIllegalMoves();
    m_searchValues.positionsSearched++;
    if constexpr (onPlyZero)
    {
        pBestMove->score = NegCheckMateScore;
    }

    if ((depth <= 0) || (ply == MaxEngineDepth))
    {
        int32 score = QuiscenceSearch<isWhite>(ply, alpha, beta);
        return score;
    }
    m_searchValues.normalSearched++;

    // Prefetch the TT before generating the check and pin masks.  Prefetching the TT data make
    // the engine ~10% faster.
    m_mainSearchTransTable.PrefetchEntry(m_pBoard->GetZobKey());

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
    ttMove = m_mainSearchTransTable.ProbeTable(m_pBoard->GetZobKey(), depth, alpha, beta);

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
            int32 futilityScore = QuiscenceSearch<isWhite>(ply, alpha, beta);
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
            int32 futilityScore = QuiscenceSearch<isWhite>(ply, alpha, beta);
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
                              (depth > settings.nullMoveDepth + 1) &&
                              (inCheck == false);

    if (canNullPrune)
    {
        int32 nullMoveScore = 0;
        settings.nullMovePrune = false;
        const uint32 depthReduction = settings.nullMoveDepth;
        m_pBoard->MakeNullMove<isWhite>();
        nullMoveScore = Negmax<!isWhite, false>(depth - depthReduction,
                                                ply + 1,
                                                nullptr,
                                                0 - beta,
                                                1 - beta,
                                                settings);
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
                               (depth > settings.mulitCutDepth + 1) &&
                               (settings.expectedCutNode == true);
    if (canDoMultiCut)
    {
        uint32 numBetaCutoffs  = 0;
        settings.multiCutPrune = false;
        uint32 numMovesDone    = 0;

        Move curMove           = GetNextMove<isWhite>(ppMoveList, &nextMoveData);

        while((curMove.fromPiece != Piece::EndOfMoveList) && (numMovesDone < settings.multiCutMoves))
        {
            numMovesDone++;
            m_pBoard->MakeMove<isWhite>(curMove);

            int32 multiCutMoveScore = Negmax<!isWhite, false>(depth - settings.mulitCutDepth,
                                                              ply + 1,
                                                              nullptr,
                                                              0 - beta,
                                                              1 - beta,
                                                              settings);
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
            curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData);
        }
        settings.multiCutPrune = true;
    }

    int32 bestScore = NegCheckMateScore + ply;
    Move  bestMove  = {};

    nextMoveData.moveIdx = 0;
    nextMoveData.moveType = MoveTypes::Best;
    Move      curMove  = GetNextMove<isWhite>(ppMoveList, &nextMoveData);

    const bool canDoLateMoveReduction = (settings.onPv == false) &&
                                        (settings.lateMoveReduction == true);

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
                if ((searchDepth <= 0) && (depth != 1))
                {
                    searchDepth = 1;
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
                                                  settings);
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
                                                    settings);
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

        settings.onPv = false;

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

        curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData);
    }

    if constexpr (onPlyZero)
    {
        *pBestMove = bestMove;
    }

    m_mainSearchTransTable.InsertToTable(m_pBoard->GetZobKey(), depth, bestMove, ttScoreType);

    return bestScore;
}

template int32 ChessEngine::Negmax<true, true>  (uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta, SearchSettings searchSettings);
template int32 ChessEngine::Negmax<true, false> (uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta, SearchSettings searchSettings);
template int32 ChessEngine::Negmax<false, true> (uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta, SearchSettings searchSettings);
template int32 ChessEngine::Negmax<false, false>(uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta, SearchSettings searchSettings);

template<bool isWhite>
int32 ChessEngine::QuiscenceSearch(uint32 ply, int32 alpha, int32 beta)
{
    m_pBoard->InvalidateCheckPinAndIllegalMoves();
    m_searchValues.positionsSearched++;
    m_searchValues.quiscenceSearched++;

    int32 standPatScore = m_pBoard->ScoreBoard<isWhite>();

    if (ply == MaxEngineDepth)
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

    m_pBoard->GenerateLegalMoves<isWhite, false>(ppMoveList);

    const bool inCheck = m_pBoard->InCheck();

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
    Move            curMove      = GetNextMove<isWhite>(ppMoveList, &nextMoveData);
    while (curMove.fromPiece != Piece::EndOfMoveList)
    {
        // Once we're just capturing pawns, break out.
        if (IsMoveGoodForQsearch(curMove, inCheck) == false)
        {
            break;
        }
        m_pBoard->MakeMove<isWhite>(curMove);
        didMove = true;
        int32 moveScore = QuiscenceSearch<!isWhite>(ply + 1, beta * -1, alpha * -1);
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
        curMove = GetNextMove<isWhite>(ppMoveList, &nextMoveData);
    }

    if (didMove)
    {
        m_qSearchTransTable.InsertToTable(m_pBoard->GetZobKey(), 0, bestMove, ttScoreType);
    }
    return bestScore;
}

std::string ChessEngine::ConvertScoreToStr(int32 score)
{
    std::string scoreStr = "";
    // detect checkmate
    if (score > (PosCheckMateScore - (int32)(2 * MaxEngineDepth)))
    {
        int32 mateDepth = (PosCheckMateScore - (score - 1)) / 2;
        scoreStr = "White has mate in ";
        scoreStr += std::to_string(mateDepth);
    }
    else if (score < (NegCheckMateScore + (int32)(2 * MaxEngineDepth)))
    {
        score *= -1;
        int32 mateDepth = (NegCheckMateScore - (score - 1)) / 2;
        scoreStr = "Black has mate in ";
        scoreStr += std::to_string(mateDepth);
    }
    else
    {
        scoreStr = std::to_string(((float)score) / 10.0);
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
void SortMoves(Move* pMoveList)
{
    uint32 i = 0;
    while (pMoveList[i].fromPiece != Piece::EndOfMoveList)
    {
        uint32 minIdx = i;
        uint32 j = i + 1;
        while (pMoveList[j].fromPiece != Piece::EndOfMoveList)
        {
            if (pMoveList[j].score < pMoveList[minIdx].score)
            {
                minIdx = j;
            }
            j++;
        }
        if (minIdx != i)
        {
            SwapMoves(pMoveList, i, minIdx);
        }
        i++;
    }
}

bool ChessEngine::IsMoveGoodForQsearch(const Move& move, bool inCheck)
{
    // don't check pawn captures in Qsearch.
    constexpr int32 cutoffScore = MVVLVA_arr[wKing][wPawn];
    return (move.score < cutoffScore) || inCheck;
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
Move ChessEngine::GetNextMove(Move** ppMoveList, GetNextMoveData* pData)
{
    Move curMove = ppMoveList[pData->moveType][pData->moveIdx];

    while (curMove.fromPiece == Piece::EndOfMoveList)
    {
        pData->moveIdx = 0;
        switch (pData->moveType)
        {
            case(MoveTypes::Best):
                // Sort Attacking moves as needed
                if (pData->sortedAttacks == false)
                {
                    SortMoves(&(ppMoveList[MoveTypes::Attack][0]));
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
            curMove = GetNextMove<isWhite>(ppMoveList, pData);
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
    data.moveIdx       = 0;
    data.moveIdx       = MoveTypes::Best;
    data.sortedAttacks = false;

    return data;
}