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
    m_pppMoveLists = static_cast<Move***>(malloc(sizeof(Move**) * MaxEngineDepth));

    for (uint32 i = 0; i < MaxEngineDepth; i++)
    {
        m_pppMoveLists[i] = static_cast<Move**>(malloc(sizeof(Move*) * MoveTypes::MoveTypeCount));
        for (uint32 j = 0; j < MoveTypes::MoveTypeCount; j++)
        {
            m_pppMoveLists[i][j] = static_cast<Move*>(malloc(sizeof(Move) * MaxMovesPerPosition));
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
            free(m_pppMoveLists[i][j]);
        }
        free(m_pppMoveLists[i]);
    }
    free(m_pppMoveLists);
    m_pppMoveLists = nullptr;
    m_mainSearchTransTable.Destroy();
    m_qSearchTransTable.Destroy();
}

void ChessEngine::SetupInitialSearchSettings(SearchSettings* pSettings)
{
    pSettings->onPv                   = true;

    pSettings->nullMovePrune          = true;
    pSettings->nullMoveDepth          = 3;

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

    std::cout << "Normal Searched      : " << m_searchValues.normalSearched << std::endl;
    std::cout << "Quiscence searched   : " << m_searchValues.quiscenceSearched << std::endl;
    std::cout << "TransTable hits      : " << m_searchValues.mainTransTableHits << std::endl;
    std::cout << "QSearch TT hits      : " << m_searchValues.qTransTableHits << std::endl;
    std::cout << "Null Move Prunes     : " << m_searchValues.nullMoveCutoffs << std::endl;
    std::cout << "Futility Prunes      : " << m_searchValues.futilityCutoffs << std::endl;
    std::cout << "Extended Fut. Prunes : " << m_searchValues.extendedFutilityCutoffs << std::endl;
    std::cout << "MultiCut Prunes      : " << m_searchValues.multiCutCutoffs << std::endl;
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
    Move* pCaptureList = &(m_pppMoveLists[ply][MoveTypes::Attack][0]);
    uint32 numCaptures = 0;
    Move* pNormalList = &(m_pppMoveLists[ply][MoveTypes::Normal][0]);
    uint32 numNormal = 0;

    m_pBoard->GenerateLegalMoves<isWhite, false>(pCaptureList, pNormalList, &numCaptures, &numNormal);
    //SortMoves(pCaptureList, numCaptures);

    const uint32 numMoves = numCaptures + numNormal;
    Move* pMoveList = pCaptureList;
    uint32 idxSubtractVal = 0;

    // We've generated the moves all the moves for the last layer already, no reason to actually count them.
    if (depth <= 1)
    {
        m_searchValues.positionsSearched += numMoves;
        return 0;
    }

    BoardInfo prevBoardData = {};
    m_pBoard->CopyBoardData(&prevBoardData);

    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));

    for (uint32 moveIdx = 0; moveIdx < numMoves; moveIdx++)
    {
        if (moveIdx == numCaptures)
        {
            pMoveList = pNormalList;
            idxSubtractVal = numCaptures;
        }
        const Move move = pMoveList[moveIdx - idxSubtractVal];

        m_pBoard->MakeMove<isWhite>(move);
        Perft<!isWhite>(depth-1, ply+1);
        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));
    }

    return 0;
}

template<bool isWhite>
void ChessEngine::PerftExpanded(uint32 depth)
{
    Move* pCaptureList = &(m_pppMoveLists[0][MoveTypes::Attack][0]);
    uint32 numCaptures = 0;
    Move* pNormalList = &(m_pppMoveLists[0][MoveTypes::Normal][0]);
    uint32 numNormal = 0;

    m_pBoard->GenerateLegalMoves<isWhite, false>(pCaptureList, pNormalList, &numCaptures, &numNormal);

    const uint32 numMoves = numCaptures + numNormal;
    Move* pMoveList = pCaptureList;
    uint32 idxSubtractVal = 0;

    BoardInfo prevBoardData = {};
    m_pBoard->CopyBoardData(&prevBoardData);

    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));


    uint32 prevNumMoves = 0;
    std::string prevMoveStr = "00";
    for (uint32 moveIdx = 0; moveIdx < numMoves; moveIdx++)
    {
        if (moveIdx == numCaptures)
        {
            pMoveList = pNormalList;
            idxSubtractVal = numCaptures;
        }
        const Move move = pMoveList[moveIdx - idxSubtractVal];

        m_pBoard->MakeMove<isWhite>(move);
        Perft<!isWhite>(depth - 1, 1);
        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        std::string moveStr = m_pBoard->GetStringFromMove(move);

        // print newline after each piece
        if ((prevMoveStr[0] != moveStr[0]) || (prevMoveStr[1] != moveStr[1]))
        {
            std::cout << std::endl;
        }
        prevMoveStr = moveStr;
        std::cout << moveStr << ": " << m_searchValues.positionsSearched - prevNumMoves << std::endl;
        prevNumMoves = m_searchValues.positionsSearched;
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
    m_searchValues.positionsSearched++;
    constexpr int32 scoreMultiplier = (isWhite) ? 1 : -1;
    if constexpr (onPlyZero)
    {
        pBestMove->score = NegCheckMateScore;
    }

    if ((depth <= 0) || (ply == MaxEngineDepth))
    {
        int32 score = QuiscenceSearch<isWhite>(ply, alpha, beta);
        //int32 score = m_pBoard->ScoreBoard();
        //score *= scoreMultiplier;
        return score;
    }
    m_searchValues.normalSearched++;

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
        ttMoveValid = m_pBoard->IsMoveLegal(ttMove);
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
                                          (depth == 2)                           &&
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

    Move* pCaptureList = &(m_pppMoveLists[ply][MoveTypes::Attack][0]);
    uint32 numCaptures = 0;
    Move* pNormalList = &(m_pppMoveLists[ply][MoveTypes::Normal][0]);
    uint32 numNormal = 0;

    // sneak the tt move onto the front of pCaptureList
    if (ttMoveValid)
    {
        pCaptureList[0] = ttMove;
        pCaptureList++;
    }

    m_pBoard->GenerateLegalMoves<isWhite, false>(pCaptureList, pNormalList, &numCaptures, &numNormal);
    SortMoves(pCaptureList, numCaptures);

    // sneak the tt move onto the front of pCaptureList
    if (ttMoveValid)
    {
        pCaptureList--;
        numCaptures++;
    }

    const uint32 numMoves = numCaptures + numNormal;
    Move* pMoveList = pCaptureList;
    uint32 idxSubtractVal = 0;


    const bool canDoMultiCut = (settings.onPv == false) &&
                               (settings.multiCutPrune == true) &&
                               (inCheck == false)                     &&
                               (depth > settings.mulitCutDepth + 1);
    if (canDoMultiCut)
    {
        uint32 numBetaCutoffs = 0;
        settings.multiCutPrune = false;
        uint32 numMultiCutMoves = (settings.multiCutMoves < numMoves) ? settings.multiCutMoves :
                                                                        numMoves;
        for (uint32 moveIdx = 0; moveIdx < numMoves; moveIdx++)
        {
            if (moveIdx == numCaptures)
            {
                pMoveList = pNormalList;
                idxSubtractVal = numCaptures;
            }
            const Move move = pMoveList[moveIdx - idxSubtractVal];
            m_pBoard->MakeMove<isWhite>(move);

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

        }
        settings.multiCutPrune = true;
    }

    int32 bestScore = NegCheckMateScore + ply;

    Move bestMove = pCaptureList[0];
    bestMove.score = bestScore;

    // reset these in case they got changed in the pruning searches
    pMoveList = pCaptureList;
    idxSubtractVal = 0;

    for (uint32 moveIdx = 0; moveIdx < numMoves; moveIdx++)
    {
        if (moveIdx == numCaptures)
        {
            pMoveList = pNormalList;
            idxSubtractVal = numCaptures;
        }
        const Move move = pMoveList[moveIdx - idxSubtractVal];
        m_pBoard->MakeMove<isWhite>(move);

        int32 moveScore = Negmax<!isWhite, false>(depth - 1, 
                                                  ply + 1,
                                                  nullptr,
                                                  beta * -1,
                                                  alpha * -1,
                                                  settings);
        // Flip the sign since this is negmax
        moveScore *= -1;

        if (bestScore < moveScore)
        {
            bestMove       = move;
            bestMove.score = moveScore;
            bestScore      = moveScore;
        }

        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        settings.onPv = false;

        if (bestScore > alpha)
        {
            alpha = bestScore;
            ttScoreType = TTScoreType::Exact;
        }

        if (alpha >= beta)
        {
            ttScoreType = TTScoreType::UpperBound;
            break;
        }
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

    m_pBoard->GenerateCheckAndPinMask<isWhite>();

    TTScoreType ttScoreType = TTScoreType::LowerBound;
    Move ttMove = m_qSearchTransTable.ProbeTable(m_pBoard->GetZobKey(), 0, alpha, beta);

    bool ttMoveValid = ttMove.score != TTScoreNotFound;
    if (ttMoveValid)
    {
        ttMoveValid = m_pBoard->IsMoveLegal(ttMove);
    }
    // TT table hit.
    if ((ttMove.score != InvalidScore) && ttMoveValid)
    {
        m_searchValues.qTransTableHits++;
        return ttMove.score;
    }

    Move* pCaptureList = &(m_pppMoveLists[ply][MoveTypes::Attack][0]);
    uint32 numCaptures = 0;
    Move* pNormalList = &(m_pppMoveLists[ply][MoveTypes::Normal][0]);
    uint32 numNormal = 0;

    m_pBoard->GenerateLegalMoves<isWhite, false>(pCaptureList, pNormalList, &numCaptures, &numNormal);
    SortMoves(pCaptureList, numCaptures);

    const uint32 numMoves = numCaptures + numNormal;
    Move* pMoveList = pCaptureList;
    uint32 idxSubtractVal = 0;

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
    for (uint32 moveIdx = 0; moveIdx < numMoves; moveIdx++)
    {
        if (moveIdx == numCaptures)
        {
            pMoveList = pNormalList;
            idxSubtractVal = numCaptures;
        }
        const Move move = pMoveList[moveIdx - idxSubtractVal];
        // Once we're just capturing pawns, break out.
        if (IsMoveGoodForQsearch(move, inCheck) == false)
        {
            break;
        }
        m_pBoard->MakeMove<isWhite>(move);
        didMove = true;
        int32 moveScore = QuiscenceSearch<!isWhite>(ply + 1, beta * -1, alpha * -1);
        // Flip the sign since this is negmax
        moveScore *= -1;

        bestScore = (bestScore < moveScore) ? moveScore : bestScore;

        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        if (bestScore > alpha)
        {
            ttScoreType = TTScoreType::Exact;
            bestMove = move;
            bestMove.score = bestScore;
            alpha = bestScore;
        }

        if (alpha >= beta)
        {
            ttScoreType = TTScoreType::UpperBound;
            break;
        }
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

void SortMoves(Move* pMoveList, uint32 numMoves)
{
    // Then sort them using a simple in place insertion sort.
    for (uint32 i = 0; i < numMoves; i++)
    {
        uint32 minIdx = i;
        for (uint32 j = i+1; j < numMoves; j++)
        {
            if (pMoveList[j].score < pMoveList[minIdx].score)
            {
                minIdx = j;
            }
        }
        if (minIdx != i)
        {
            SwapMoves(pMoveList, i, minIdx);
        }
    }
}

bool ChessEngine::IsMoveGoodForQsearch(const Move& move, bool inCheck)
{
    // don't check pawn captures in Qsearch.
    constexpr int32 cutoffScore = MVVLVA_arr[wKing][wPawn];
    return (move.score < cutoffScore) || inCheck;
}