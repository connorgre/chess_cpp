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

void ChessEngine::DoEngine(uint32 depth, bool isWhite)
{
    m_positionsSearched = 0ull;
    Move bestMove = {};
    int32 bestScore = 0;
    auto startTime = std::chrono::steady_clock::now();

    if (isWhite)
    {
        bestScore = Negmax<true, true>(depth, 0, &bestMove, InitialAlpha, InitialBeta);
    }
    else
    {
        bestScore = Negmax<false, true>(depth, 0, &bestMove, InitialAlpha, InitialBeta);
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
    Move* pMoveList = &(m_moveLists[ply][0]);
    uint32 numMoves = 0;

    m_pBoard->GenerateLegalMoves<isWhite>(pMoveList, &numMoves);

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
        const Move move = pMoveList[moveIdx];
        m_pBoard->MakeMove<isWhite>(move);
        Perft<!isWhite>(depth-1, ply+1);
        m_pBoard->UndoMove(&prevBoardData, &(prevBoardPieces[0]));
    }

    return 0;
}

template<bool isWhite>
void ChessEngine::PerftExpanded(uint32 depth)
{
    Move* pMoveList = &(m_moveLists[0][0]);
    uint32 numMoves = 0;


    m_pBoard->GenerateLegalMoves<isWhite>(pMoveList, &numMoves);

    BoardInfo prevBoardData = {};
    m_pBoard->CopyBoardData(&prevBoardData);

    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));


    uint32 prevNumMoves = 0;
    std::string prevMoveStr = "00";
    for (uint32 moveIdx = 0; moveIdx < numMoves; moveIdx++)
    {
        const Move move = pMoveList[moveIdx];

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
        // When I add qSearch, I shouldn't multiply here...
        int32 score = scoreMultiplier * m_pBoard->ScoreBoard();
        score -= ply;   // encourage finding better moves sooner
        return score;
    }

    Move* pMoveList = &(m_moveLists[ply][0]);
    uint32 numMoves = 0;

    m_pBoard->GenerateLegalMoves<isWhite>(pMoveList, &numMoves);

    BoardInfo prevBoardData = {};
    uint64 prevBoardPieces[static_cast<uint32>(Piece::PieceCount)];
    m_pBoard->CopyBoardData(&prevBoardData);
    m_pBoard->CopyPieceData(&(prevBoardPieces[0]));

    int32 bestScore = NegCheckMateScore + ply;
    for (uint32 moveIdx = 0; moveIdx < numMoves; moveIdx++)
    {
        const Move move = pMoveList[moveIdx];
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
        scoreStr = std::to_string(score);
    }
    return scoreStr;
}