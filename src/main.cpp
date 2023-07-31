#include <iostream>
#include "../inc/util.h"
#include "../inc/board.h"
#include "../inc/chess.h"
int main(int argc, char** argv)
{
    std::cout << "hi" << std::endl;

    Board board = Board();
    board.Init();
    board.PrintBoard();

    ChessGame game = ChessGame();
    game.Init();
    game.Run();

    return 0;
}