#include "../inc/engine.h"
#include "../inc/util.h"
#include "../inc/board.h"
#include <chrono>

ChessEngine::ChessEngine()
:
m_pBoard(nullptr),
m_moveLists(),
m_positionsSearched(0)
{

}

ChessEngine::~ChessEngine()
{

}

void ChessEngine::Init(Board* pBoard)
{
    m_pBoard = pBoard;
}

void ChessEngine::Destroy()
{

}

void ChessEngine::DoEngine(uint32 depth, bool isWhite, bool doMove)
{
    m_positionsSearched = 0ull;
    m_quiscenceSearched = 0ull;
    Move bestMove = {};
    int32 bestScore = 0;
    auto startTime = std::chrono::steady_clock::now();

    if (isWhite)
    {
        bestScore = Negmax<true, true>(depth, 0, &bestMove, InitialAlpha, InitialBeta);
        if (doMove)
        {
            m_pBoard->MakeMove<true>(bestMove);
        }
    }
    else
    {
        bestScore = Negmax<false, true>(depth, 0, &bestMove, InitialAlpha, InitialBeta);
        if (doMove)
        {
            m_pBoard->MakeMove<false>(bestMove);
        }
        // Need to invert the score since black will return the hightest value because of negmax,
        // however outside negmax, negative score is better for black.
        bestScore      *= -1;
        bestMove.score *= -1;
    }

    auto endTime = std::chrono::steady_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    uint32 knps = 0;
    if (totalTime.count() > 0)
    {
        knps = m_positionsSearched / totalTime.count();
    }

    std::string bestMoveStr = m_pBoard->GetStringFromMove(bestMove);
    CH_ASSERT(bestScore == bestMove.score);

    std::string scoreStr = ConvertScoreToStr(bestScore);

    std::cout << "Best Move         : " << bestMoveStr << std::endl;
    std::cout << "Score             : " << scoreStr << std::endl;

    std::cout << "Time              : " << totalTime.count() << " ms" << std::endl;
    std::cout << "Positions searched: " << m_positionsSearched << std::endl;
    std::cout << "Knps              : " << knps << std::endl;

    std::cout << "Quiscence searched: " << m_quiscenceSearched << std::endl;
}

void ChessEngine::DoPerft(uint32 depth, bool isWhite, bool expanded)
{
    m_positionsSearched = 0ull;
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
        knps = m_positionsSearched / totalTime.count();
    }

    std::cout << "Time              : " << totalTime.count() << " ms" << std::endl;
    std::cout << "Positions searched: " << m_positionsSearched << std::endl;
    std::cout << "Knps              : " << knps << std::endl;
}

template<bool isWhite>
uint32 ChessEngine::Perft(uint32 depth, uint32 ply)
{
    Move* pCaptureList = &(m_moveLists[ply][MoveTypes::Attack][0]);
    uint32 numCaptures = 0;
    Move* pNormalList = &(m_moveLists[ply][MoveTypes::Normal][0]);
    uint32 numNormal = 0;

    m_pBoard->GenerateLegalMoves<isWhite, false>(pCaptureList, pNormalList, &numCaptures, &numNormal);
    //SortMoves(pCaptureList, numCaptures);

    const uint32 numMoves = numCaptures + numNormal;
    Move* pMoveList = pCaptureList;
    uint32 idxSubtractVal = 0;

    // We've generated the moves all the moves for the last layer already, no reason to actually count them.
    if (depth <= 1)
    {
        m_positionsSearched += numMoves;
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
    Move* pCaptureList = &(m_moveLists[0][MoveTypes::Attack][0]);
    uint32 numCaptures = 0;
    Move* pNormalList = &(m_moveLists[0][MoveTypes::Normal][0]);
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
        std::cout << moveStr << ": " << m_positionsSearched - prevNumMoves << std::endl;
        prevNumMoves = m_positionsSearched;
    }
}

void ChessEngine::ResetPerftStats()
{
    m_positionsSearched = 0;
}

template uint32 ChessEngine::Perft<true>(uint32 depth, uint32 ply);
template uint32 ChessEngine::Perft<false>(uint32 depth, uint32 ply);
template void ChessEngine::PerftExpanded<true>(uint32 depth);
template void ChessEngine::PerftExpanded<false>(uint32 depth);

template<bool isWhite, bool onPlyZero>
int32 ChessEngine::Negmax(uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta)
{
    m_positionsSearched++;
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

    Move* pCaptureList = &(m_moveLists[ply][MoveTypes::Attack][0]);
    uint32 numCaptures = 0;
    Move* pNormalList = &(m_moveLists[ply][MoveTypes::Normal][0]);
    uint32 numNormal = 0;

    m_pBoard->GenerateLegalMoves<isWhite, false>(pCaptureList, pNormalList, &numCaptures, &numNormal);
    SortMoves(pCaptureList, numCaptures);

    const uint32 numMoves = numCaptures + numNormal;
    Move* pMoveList = pCaptureList;
    uint32 idxSubtractVal = 0;

    BoardInfo prevBoardData = {};
    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyBoardData(&prevBoardData);
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));

    int32 bestScore = NegCheckMateScore + ply;


    for (uint32 moveIdx = 0; moveIdx < numMoves; moveIdx++)
    {
        if (moveIdx == numCaptures)
        {
            pMoveList = pNormalList;
            idxSubtractVal = numCaptures;
        }
        const Move move = pMoveList[moveIdx - idxSubtractVal];
        m_pBoard->MakeMove<isWhite>(move);

        int32 moveScore = Negmax<!isWhite, false>(depth - 1, ply + 1, nullptr, beta * -1, alpha * -1);
        // Flip the sign since this is negmax
        moveScore *= -1;

        // we only fill out the pBestMove on ply 0
        if constexpr (onPlyZero)
        {
            if (bestScore < moveScore)
            {
                memcpy(pBestMove, &move, sizeof(Move));
                pBestMove->score = moveScore;
                bestScore = moveScore;
            }
        }
        else
        {
            bestScore = (bestScore < moveScore) ? moveScore : bestScore;
        }

        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        alpha = (moveScore > alpha) ? moveScore : alpha;

        if (alpha >= beta)
        {
            break;
        }
    }

    return bestScore;
}

template int32 ChessEngine::Negmax<true, true>(uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta);
template int32 ChessEngine::Negmax<true, false>(uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta);
template int32 ChessEngine::Negmax<false, true>(uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta);
template int32 ChessEngine::Negmax<false, false>(uint32 depth, uint32 ply, Move* pBestMove, int32 alpha, int32 beta);

template<bool isWhite>
int32 ChessEngine::QuiscenceSearch(uint32 ply, int32 alpha, int32 beta)
{
    constexpr int32 scoreMultiplier = (isWhite) ? 1 : -1;
    int32 standPatScore = m_pBoard->ScoreBoard();
    standPatScore *= scoreMultiplier;
    standPatScore -= ply;

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
    m_quiscenceSearched++;

    Move* pCaptureList = &(m_moveLists[ply][MoveTypes::Attack][0]);
    uint32 numCaptures = 0;
    Move* pNormalList = &(m_moveLists[ply][MoveTypes::Normal][0]);
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
    alpha = (bestScore > alpha) ? bestScore : alpha;
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

        int32 moveScore = QuiscenceSearch<!isWhite>(ply + 1, beta * -1, alpha * -1);
        // Flip the sign since this is negmax
        moveScore *= -1;

        bestScore = (bestScore < moveScore) ? moveScore : bestScore;

        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));

        alpha = (bestScore > alpha) ? bestScore : alpha;

        if (alpha >= beta)
        {
            break;
        }
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