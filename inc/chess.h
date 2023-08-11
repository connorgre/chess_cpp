#pragma once

#include "board.h"
#include "util.h"
#include "engine.h"
#include <map>
#include <vector>

static constexpr uint32 MaxCommandLength = 512;
static constexpr uint32 MaxFenStrLength = 128;

enum class Commands : uint32
{
    Move,
    Reset,
    Quit,
    Print,
    None,
    Undo,
    Perft,
    Engine,

    NumCommands,
};

typedef std::map<std::string, Commands> CommandMap;

struct InputCommand
{
    Commands command;
    char inputString[MaxCommandLength];
    union
    {
        struct
        {
            uint32 fromIdx;
            uint32 toIdx;
            bool   isWhite;
            Move   boardMove;
        } move;

        struct
        {
            uint64 pieces;
        } print;

        struct
        {
            bool isWhite;
            uint32 depth;
            bool expanded;
        } perft;

        struct
        {
            EngineSettings settings;
        } engine;

        struct
        {
            bool isTTReset;
            char fenStr[MaxFenStrLength];
        } reset;
    };
};

class ChessGame
{
public:
    ChessGame();
    ~ChessGame();

    Result Init();
    Result Destroy();

    void Run();

private:
    Result HandleInput();
    InputCommand ParseInput(std::string input);
    void GenerateCommandMap();
    Result ParseMoveCommand(
        std::vector<std::string> commandVec,
        InputCommand* pInputCommand
        );

    Result ParsePrintCommand(
        std::vector<std::string> commandVec,
        InputCommand* pInputCommand
    );

    Result ParsePerftCommand(
        std::vector<std::string> commandVec,
        InputCommand* pInputCommand
    );

    Result ParseResetCommand(
        std::vector<std::string> commandVec,
        InputCommand* pInputCommand
    );

    Result ParseEngineCommand(
        std::vector<std::string> commandVec,
        InputCommand* pInputCommand
    );

    CommandMap         m_commandMap;
    Board              m_board;
    std::vector<Board> m_historyVec;
    ChessEngine        m_engine;
};