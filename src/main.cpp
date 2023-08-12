#include <iostream>
#include "../inc/util.h"
#include "../inc/board.h"
#include "../inc/chess.h"
#include "../inc/bitHelper.h"

int main(int argc, char** argv)
{
    std::cout << "king: ";
    std::cout << u8"♔" << std::endl;
    Board board = Board();
    board.Init();
    board.PrintBoard(board.GetAllPieces());

    ChessGame game = ChessGame();
    game.Init();
    game.Run();

    game.Destroy();

    return 0;
}