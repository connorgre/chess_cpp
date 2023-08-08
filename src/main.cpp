#include <iostream>
#include "../inc/util.h"
#include "../inc/board.h"
#include "../inc/chess.h"
#include "../inc/bitHelper.h"

int main(int argc, char** argv)
{
    std::cout << "hi" << std::endl;

    uint64 mask0 = 0ull;
    uint64 mask1 = 1ull;
    uint64 mask2 = 1ull << 63;

    uint32 index1; uint32 index2; uint32 index3;

    std::cout << mask0 << " : " << GetIndex(mask0) << std::endl;
    std::cout << mask1 << " : " << GetIndex(mask1) << std::endl;
    std::cout << mask2 << " : " << GetIndex(mask2) << std::endl;

    Board board = Board();
    board.Init();
    board.PrintBoard(board.GetAllPieces());

    ChessGame game = ChessGame();
    game.Init();
    game.Run();

    return 0;
}