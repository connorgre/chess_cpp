#include "../inc/board.h"
#include "../inc/chess.h"
#include "../inc/bitHelper.h"
#include <sstream>
#include <vector>

ChessGame::ChessGame()
{

}

ChessGame::~ChessGame()
{

}

Result ChessGame::Init()
{
    m_board.Init();

    m_historyVec.push_back(m_board);
    GenerateCommandMap();
    return Result::ErrorNotImplemented;
}

Result ChessGame::Destroy()
{
    return Result::ErrorNotImplemented;
}

void ChessGame::Run()
{
    bool running = true;
    while(running)
    {
        std::cout << ">> " << std::flush;
        std::string inputLine;
        std::getline(std::cin, inputLine);
        InputCommand command = ParseInput(inputLine);

        switch(command.command)
        {
            case (Commands::Move):
                if (command.move.isWhite)
                {
                    m_board.MakeMove<true>(command.move.boardMove);
                }
                else
                {
                    m_board.MakeMove<false>(command.move.boardMove);
                }
                break;
            case (Commands::Reset):
                m_board.ResetBoard();
                break;
            case (Commands::Quit):
                running = false;
                break;
            case (Commands::Print):
                m_board.PrintBoard();
            case (Commands::None):
                break;
            case (Commands::Undo):
                if (m_historyVec.size() > 1)
                {
                    m_historyVec.pop_back();
                    m_board = m_historyVec.back();
                }
                else
                {
                    std::cout << "Nothing to Undo" << std::endl;
                }
                break;
            default:
                CH_ASSERT(false);
        }
        if (memcmp(&m_board, &m_historyVec.back(), sizeof(Board)) != 0)
        {
            m_historyVec.push_back(m_board);
        }
    }
}

InputCommand ChessGame::ParseInput(std::string inputStr)
{
    CH_ASSERT(inputStr.length() < MaxCommandLength);
    // First, make the string lowercase
    for (uint32 idx = 0; idx < inputStr.length(); idx++)
    {
        if ((inputStr[idx] > 'A') && (inputStr[idx] <= 'Z'))
        {
            inputStr[idx] += 0x20;
        }
    }

    std::istringstream strStream(inputStr);
    std::vector<std::string> inputWords;

    std::string word;
    while (strStream >> word)
    {
        inputWords.push_back(word);
    }
    Result result = Result::Success;

    // This means just whitespace was entered, do nothing.
    if (inputWords.size() == 0)
    {
        inputWords.push_back("none");
    }

    InputCommand inputCommand = {};
    if (m_commandMap.find(inputWords[0]) == m_commandMap.end())
    {
        result = Result::ErrorInvalidInput;
    }
    else
    {
        inputCommand.command = m_commandMap[inputWords[0]];
    }

    if (result == Result::Success)
    {
        memcpy(inputCommand.inputString, inputStr.c_str(), inputStr.length());
        switch(inputCommand.command)
        {
            case (Commands::Move):
                result = ParseMoveCommand(inputWords, &inputCommand);
                break;
            case (Commands::Reset):
                CH_MESSAGE("Should add extra reset logic later")
                break;
            case (Commands::Quit):
                break;
            case (Commands::Print):
                CH_MESSAGE("Should add extra print options")
                break;
            case (Commands::None):
                break;
            case (Commands::Undo):
                break;
            default:
                CH_ASSERT(false);
                result = Result::ErrorInvalidInput;
        }
    }

    if (result != Result::Success)
    {
        inputCommand.command = Commands::None;
        CH_ASSERT(false);
    }
    return inputCommand;
}

void ChessGame::GenerateCommandMap()
{
    m_commandMap["move"]  = Commands::Move;
    m_commandMap["reset"] = Commands::Reset;
    m_commandMap["quit"]  = Commands::Quit;
    m_commandMap["exit"]  = Commands::Quit;
    m_commandMap["print"] = Commands::Print;
    m_commandMap["none"]  = Commands::None;
    m_commandMap["undo"]  = Commands::Undo;
}

Result ChessGame::ParseMoveCommand(
    std::vector<std::string> wordVec,
    InputCommand* pInputCommand)
{
    Result result = Result::Success;
    CH_ASSERT(wordVec[0] == "move");
    pInputCommand->command = Commands::Move;
    pInputCommand->move.boardMove.flags = MoveFlags::NoFlag;

    if ((wordVec.size() == 3) || (wordVec.size() == 4))
    {
        std::string fromStr = wordVec[1];
        std::string toStr   = wordVec[2];

        uint32 fromFile = fromStr[0] - 'a';
        uint32 fromRank = fromStr[1] - '1';
        uint32 fromIdx  = fromFile + fromRank * 8;
        uint64 fromPos = 1ull << fromIdx;

        uint32 toFile   = toStr[0] - 'a';
        uint32 toRank   = toStr[1] - '1';
        uint32 toIdx    = toFile + toRank * 8;
        uint64 toPos    = 1ull << toIdx;

        if ((fromIdx < 0) || (fromIdx > 63) ||
            (toIdx < 0) || (toIdx > 63))
        {
            result = Result::ErrorInvalidInput;
        }

        pInputCommand->move.fromIdx           = fromIdx;
        pInputCommand->move.boardMove.fromPos = fromPos;
        pInputCommand->move.boardMove.fromPiece = m_board.GetPieceFromPos(fromPos);

        pInputCommand->move.toIdx           = toIdx;
        pInputCommand->move.boardMove.toPos = toPos;
        pInputCommand->move.boardMove.toPiece = m_board.GetPieceFromPos(toPos);

        const Piece fromPiece = pInputCommand->move.boardMove.fromPiece;
        if (((fromPiece == wPawn) || (fromPiece == bPawn))&&
            toPos == m_board.GetEnPassantPos())
        {
            pInputCommand->move.boardMove.flags = MoveFlags::EnPassant;
        }

        CH_MESSAGE("Need to implement Promotions");

        if (pInputCommand->move.boardMove.fromPiece == Piece::NoPiece)
        {
            result = Result::ErrorInvalidInput;
        }

        pInputCommand->move.isWhite = IsWhitePiece(fromPiece);
        if (wordVec.size() == 4)
        {
            CH_ASSERT(pInputCommand->move.boardMove.flags == MoveFlags::NoFlag);
            MoveFlags flag = MoveFlags::NoFlag;
            std::string piece = wordVec[3];
            if      ((piece == "q") || (piece == "queen"))  flag = MoveFlags::QueenPromotion;
            else if ((piece == "r") || (piece == "rook"))   flag = MoveFlags::RookPromotion;
            else if ((piece == "b") || (piece == "bishop")) flag = MoveFlags::BishopPromotion;
            else if ((piece == "n") || (piece == "knight")) flag = MoveFlags::KnightPromotion;
            else CH_ASSERT(false);
            pInputCommand->move.boardMove.flags = flag;
        }
    }
    else if (wordVec.size() == 2)
    {
        CH_MESSAGE("Need to implement castling");
        if (wordVec[1] == "wkc")
        {
            pInputCommand->move.boardMove.flags = MoveFlags::WhiteKingCastle;
            pInputCommand->move.isWhite = true;
        }
        else if (wordVec[1] == "wqc")
        {
            pInputCommand->move.boardMove.flags = MoveFlags::WhiteQueenCastle;
            pInputCommand->move.isWhite = true;
        }
        else if (wordVec[1] == "bkc")
        {
            pInputCommand->move.boardMove.flags = MoveFlags::BlackKingCastle;
            pInputCommand->move.isWhite = false;
        }
        else if (wordVec[1] == "bqc")
        {
            pInputCommand->move.boardMove.flags = MoveFlags::BlackQueenCastle;
            pInputCommand->move.isWhite = false;
        }
    }
    else
    {
        result = Result::ErrorInvalidInput;
    }
    CH_ASSERT(result == Result::Success);
    return result;
}
