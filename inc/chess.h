#pragma once

#include "board.h"
#include "util.h"
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
            char fenStr[MaxFenStrLength];
        } resetFen;
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

    CommandMap         m_commandMap;
    Board              m_board;
    std::vector<Board> m_historyVec;
};