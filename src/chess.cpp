#include "../inc/board.h"
#include "../inc/chess.h"
#include "../inc/bitHelper.h"
#include "../inc/engine.h"
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

    m_engine.Init(&m_board);

    return Result::ErrorNotImplemented;
}

Result ChessGame::Destroy()
{
    m_board.Destroy();
    m_engine.Destroy();
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

        Move moveList[128] = {};
        uint32 numMoves = 0;

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
                if (command.reset.isTTReset)
                {
                    m_engine.ResetTransTable();
                }
                else
                {
                    m_board.SetBoardFromFEN(command.reset.fenStr);
                }
                break;
            case (Commands::Quit):
                running = false;
                break;
            case (Commands::Print):
                m_board.PrintBoard(command.print.pieces);
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
            case (Commands::Perft):
                m_engine.DoPerft(command.perft.depth, command.perft.isWhite, command.perft.expanded);
                break;
            case (Commands::Engine):
                m_engine.DoEngine(command.engine.depth, command.engine.isWhite, command.engine.doMove);
                break;
            default:
                CH_ASSERT(false);
        }
        if (memcmp(&m_board, &m_historyVec.back(), sizeof(Board)) != 0)
        {
            m_historyVec.push_back(m_board);
        }
        std::cout << std::flush;
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
                result = ParseResetCommand(inputWords, &inputCommand);
                break;
            case (Commands::Quit):
                break;
            case (Commands::Print):
                result = ParsePrintCommand(inputWords, &inputCommand);
                break;
            case (Commands::None):
                break;
            case (Commands::Undo):
                break;
            case (Commands::Perft):
                result = ParsePerftCommand(inputWords, &inputCommand);
                break;
            case (Commands::Engine):
                result = ParseEngineCommand(inputWords, &inputCommand);
                break;
            default:
                CH_ASSERT(false);
                result = Result::ErrorInvalidInput;
        }
    }

    if (result != Result::Success)
    {
        inputCommand.command = Commands::None;
    }
    return inputCommand;
}

void ChessGame::GenerateCommandMap()
{
    m_commandMap["move"]   = Commands::Move;
    m_commandMap["reset"]  = Commands::Reset;
    m_commandMap["quit"]   = Commands::Quit;
    m_commandMap["exit"]   = Commands::Quit;
    m_commandMap["print"]  = Commands::Print;
    m_commandMap["none"]   = Commands::None;
    m_commandMap["undo"]   = Commands::Undo;
    m_commandMap["perft"]  = Commands::Perft;
    m_commandMap["engine"] = Commands::Engine;
    m_commandMap["search"] = Commands::Engine;
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

Result ChessGame::ParsePrintCommand(
    std::vector<std::string> wordVec,
    InputCommand* pInputCommand)
{
    uint64 pieces = 0ull;
    uint32 vecLen = wordVec.size();

    Result result = Result::Success;

    if (vecLen == 1)
    {
        pieces = m_board.GetAllPieces();
    }
    else if (vecLen == 2)
    {
        if (wordVec[1] == "black")
        {
            pieces = m_board.GetBlackPieces();
        }
        else if (wordVec[1] == "white")
        {
            pieces = m_board.GetWhitePieces();
        }
        else
        {
            result = Result::ErrorInvalidInput;
        }
    }
    else if (vecLen == 3)
    {
        if (wordVec[1] == "legal")
        {
            std::string pieceStr = wordVec[2];
            if (pieceStr.length() == 2)
            {
                uint32 pieceFile = pieceStr[0] - 'a';
                uint32 pieceRank = pieceStr[1] - '1';
                uint32 pieceIdx = pieceFile + pieceRank * 8;
                uint64 piecePos = 0ull;
                if (pieceIdx > 63)
                {
                    result = Result::ErrorInvalidInput;
                }
                else
                {
                    piecePos = 1ull << pieceIdx;
                }
                pieces = m_board.GetLegalMoves(piecePos);
            }
            else
            {
                result = Result::ErrorInvalidInput;
            }
        }
        else
        {
            result = Result::ErrorInvalidInput;
        }
    }

    pInputCommand->print.pieces = pieces;
    return result;
}

Result ChessGame::ParsePerftCommand(
    std::vector<std::string> wordVec,
    InputCommand* pInputCommand)
{
    Result result = Result::Success;
    // default to perft from white
    pInputCommand->perft.isWhite = true;
    pInputCommand->perft.depth = UINT32_MAX;
    pInputCommand->perft.expanded = false;
    uint32 size = wordVec.size();

    for (uint32 word = 1; word < size; word++)
    {
        if (wordVec[word].length() == 1)
        {
            pInputCommand->perft.depth = wordVec[word][0] - '0';
        }
        else if (wordVec[word] == "black")
        {
            pInputCommand->perft.isWhite = false;
        }
        else if (wordVec[word] == "expand")
        {
            pInputCommand->perft.expanded = true;
        }
        else
        {
            result = Result::ErrorInvalidInput;
        }
    }
    if (pInputCommand->perft.depth > 9)
    {
        result = Result::ErrorInvalidInput;
    }
    return result;
}

bool IsInteger(std::string str)
{
    for (char c : str)
    {
        if (std::isdigit(c) == false)
        {
            return false;
        }
    }
    return true;
}

Result ChessGame::ParseEngineCommand(
    std::vector<std::string> wordVec,
    InputCommand* pInputCommand)
{
    Result result = Result::Success;
    bool colorSpecified = false;;
    pInputCommand->engine.isWhite = true;
    pInputCommand->engine.depth = UINT32_MAX;
    pInputCommand->engine.doMove = false;
    uint32 size = wordVec.size();

    for (uint32 word = 1; word < size; word++)
    {
        if (IsInteger(wordVec[word]))
        {
            pInputCommand->engine.depth = std::stoi(wordVec[word]);
        }
        else if (wordVec[word] == "black")
        {
            colorSpecified = true;
            pInputCommand->engine.isWhite = false;
        }
        else if (wordVec[word] == "white")
        {
            colorSpecified = true;
            pInputCommand->engine.isWhite = true;
        }
        else if (wordVec[word] == "move")
        {
            pInputCommand->engine.doMove = true;
        }
        else
        {
            result = Result::ErrorInvalidInput;
        }
    }
    if (pInputCommand->engine.depth > MaxEngineDepth)
    {
        result = Result::ErrorInvalidInput;
    }

    if (colorSpecified == false)
    {
        result = Result::ErrorInvalidInput;
    }
    return result;
}

Result ChessGame::ParseResetCommand(
    std::vector<std::string> wordVec,
    InputCommand* pInputCommand)
{
    Result result = Result::Success;
    uint32 vecLen = wordVec.size();
    pInputCommand->reset.isTTReset = false;

    // https://www.chess.com/analysis?tab=analysis
    if (vecLen == 1)
    {
        char fenStr[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
        memcpy(pInputCommand->reset.fenStr, fenStr, sizeof(fenStr));
    }
    else if (vecLen == 2)
    {
        if (wordVec[1] == "kiwipete")
        {
            char fenStr[] = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
            memcpy(pInputCommand->reset.fenStr, fenStr, sizeof(fenStr));
        }
        else if (wordVec[1] == "1")
        {
            char fenStr[] = "3qkr/3pp1/6P/7B/8/8/P7/K7 b - -";
            memcpy(pInputCommand->reset.fenStr, fenStr, sizeof(fenStr));
        }
        else if (wordVec[1] == "2")
        {
            // stockfish says -.9, correct move is a6e1
            char fenStr[] = "r4rk1/p4ppp/Bppp2n1/7q/4bP2/6P1/PPPQ3P/R1B2RK1 w -";
            memcpy(pInputCommand->reset.fenStr, fenStr, sizeof(fenStr));
        }
        else if (wordVec[1] == "3")
        {
            // stockfish says +1.7 e8c8
            char fenStr[] = "1kq1Q3/pp5p/6p1/1Np2p2/8/P2P2b1/1PPB1nK1/8 w - - 1 26";
            memcpy(pInputCommand->reset.fenStr, fenStr, sizeof(fenStr));
        }
        else if (wordVec[1] == "4")
        {
            // stockfish says +.4 g5f3
            char fenStr[] = "b3nrk1/8/5q1p/2p1N1N1/p1P1P3/P2PQ2P/2P2PP1/7K w - - 1 26";
            memcpy(pInputCommand->reset.fenStr, fenStr, sizeof(fenStr));
        }
        else if ((wordVec[1] == "tt") || (wordVec[1] == "transtable"))
        {
            pInputCommand->reset.isTTReset = true;
        }
        else
        {
            result = Result::ErrorInvalidInput;
        }
    }
    else
    {
        result = Result::ErrorInvalidInput;
    }
    return result;
}