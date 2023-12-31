#include "../inc/board.h"
#include "../inc/util.h"
#include "../inc/bitHelper.h"
#include <random>
Board::Board()
:
m_pieces(),
m_boardState(),
m_pRayTable(),
m_ppZobristArray(nullptr),
m_prevZobKeyVec(),
m_fancyPrint(false)
{
    constexpr uint32 PrevZobKeyVecLength = 1024;
    // unlikely a game will go for more than 512 moves... if it does we segfault...
    m_prevZobKeyVec.reserve(PrevZobKeyVecLength);
    for (uint32 idx = 0; idx < PrevZobKeyVecLength; idx++)
    {
        m_prevZobKeyVec.push_back(0ull);
    }
}

Board::~Board()
{

}

Result Board::Init()
{
    InitZobArray();
    ResetBoard();
    GenerateRayTable();
    ResetPieceScore();

    return Result::ErrorNotImplemented;
}

Result Board::Destroy()
{
    for (uint32 idx = 0; idx < Piece::PieceCount; idx++)
    {
        free(m_ppZobristArray[idx]);
    }
    free(m_ppZobristArray);
    m_ppZobristArray = nullptr;
    return Result::ErrorNotImplemented;
}

void Board::SetBoardFromFEN(std::string fenStr)
{
    uint32 fenStrIdx       = 0;
    const uint32 fenStrLen = fenStr.length();

    m_boardState = {};

    for (uint32 idx = 0; idx < Piece::PieceCount; idx++)
    {
        m_pieces[idx] = 0;
    }

    // Fen starts at top left corner of board (A8)
    uint32 boardIdx  = A8;
    uint32 extraLine = 0; 
    while ((boardIdx >= A1) && (fenStr[fenStrIdx] != ' '))
    {
        switch (fenStr[fenStrIdx])
        {
            case 'K':
                m_pieces[Piece::wKing]   |= IndexToPosition(boardIdx);
                break;
            case 'Q':
                m_pieces[Piece::wQueen]  |= IndexToPosition(boardIdx);
                break;
            case 'R':
                m_pieces[Piece::wRook]   |= IndexToPosition(boardIdx);
                break;
            case 'B':
                m_pieces[Piece::wBishop] |= IndexToPosition(boardIdx);
                break;
            case 'N':
                m_pieces[Piece::wKnight] |= IndexToPosition(boardIdx);
                break;
            case 'P':
                m_pieces[Piece::wPawn]   |= IndexToPosition(boardIdx);
                break;
            case 'k':
                m_pieces[Piece::bKing]   |= IndexToPosition(boardIdx);
                break;
            case 'q':
                m_pieces[Piece::bQueen]  |= IndexToPosition(boardIdx);
                break;
            case 'r':
                m_pieces[Piece::bRook]   |= IndexToPosition(boardIdx);
                break;
            case 'b':
                m_pieces[Piece::bBishop] |= IndexToPosition(boardIdx);
                break;
            case 'n':
                m_pieces[Piece::bKnight] |= IndexToPosition(boardIdx);
                break;
            case 'p':
                m_pieces[Piece::bPawn]   |= IndexToPosition(boardIdx);
                break;
            case '/':
                // Handle the case where we reached the end of the row, as the
                // next line would just subtract 0 instead of 8
                extraLine = (boardIdx % 8 == 0) ? 8 : 0;
                boardIdx -= boardIdx % 8;
                boardIdx -= (8 + extraLine);

                // Need continue here to avoid extra boardIdx++ at the end
                // of the loop
                fenStrIdx++;
                continue;
            default:
                // Default is when there is a number, indicating empty space
                boardIdx += (uint32)(fenStr[fenStrIdx] - '0');

                // If we reached the end of the row, next char is a /, handle
                // that here
                if ((boardIdx % 8) == 0)
                {
                    boardIdx -= 16;
                    fenStrIdx++;
                }
                if (fenStr[fenStrIdx] != ' ')
                {
                    fenStrIdx++;
                }
                // don't want to increment boardIdx again
                continue;
        } // switch (fenStr[fenStrIdx])
        if (fenStr[fenStrIdx] != ' ')
        {
            fenStrIdx++;
        }
        boardIdx++;
    }

    while (fenStr[fenStrIdx] == ' ')
    {
        fenStrIdx++;
    }

    m_boardState.isWhiteTurn = true;
    if (fenStr[fenStrIdx] == 'b')
    {
        m_boardState.isWhiteTurn = false;
    }
    else
    {
        CH_ASSERT(fenStr[fenStrIdx] == 'w');
    }

    fenStrIdx++;
    fenStrIdx++;

    // handle castling
    m_boardState.castleMask = 0;
    while ((fenStr[fenStrIdx] != ' ') && (fenStr[fenStrIdx] != '-'))
    {
        if (fenStr[fenStrIdx] == 'K')
        {
            m_boardState.castleMask |= MoveFlags::WhiteKingCastle;
        }
        else if (fenStr[fenStrIdx] == 'Q')
        {
            m_boardState.castleMask |= MoveFlags::WhiteQueenCastle;
        }
        else if (fenStr[fenStrIdx] == 'k')
        {
            m_boardState.castleMask |= MoveFlags::BlackKingCastle;
        }
        else if (fenStr[fenStrIdx] == 'q')
        {
            m_boardState.castleMask |= MoveFlags::BlackQueenCastle;
        }
        else
        {
            CH_ASSERT(false);
        }
        fenStrIdx++;
    }

    // handle EP here
    CH_MESSAGE("Finish En Passant");

    // Finish setting up board here
    m_boardState.whitePieces = 0ull;
    m_boardState.blackPieces = 0ull;
    for (uint32 idx = 0; idx < Piece::PieceCount; idx++)
    {
        if (IsWhitePiece(static_cast<Piece>(idx)))
        {
            m_boardState.whitePieces |= m_pieces[idx];
        }
        else
        {
            CH_ASSERT(IsBlackPiece(static_cast<Piece>(idx)));
            m_boardState.blackPieces |= m_pieces[idx];
        }
    }
    m_boardState.allPieces = m_boardState.whitePieces | m_boardState.blackPieces;

    m_boardState.zobristKey = 0ull;
    ResetZobKey();
    ResetPieceScore();
}

void Board::ResetBoard()
{
    std::string fenStr = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
    SetBoardFromFEN(fenStr);
}

void Board::PrintBoard(uint64 pieces)
{
    char pieceBuf[8][8];
    for (uint32 x = 0; x < 8; x++)
    {
        for (uint32 y = 0; y < 8; y++)
        {
            pieceBuf[x][y] = '.';
        }
    }

    // List is of length 1 longer than the most bits in a uint64.
    uint64 pieceList[65] = {};
    GetIndividualBits(pieces, pieceList);
    uint32 idx = 0;
    while (pieceList[idx] != 0ull)
    {
        uint64 piece = pieceList[idx];
        uint32 file = GetFile(piece);
        uint32 rank = GetRank(piece);
        pieceBuf[file][rank] = 'x';

        if      (IsWhiteKing(piece))   pieceBuf[file][rank] = 'K';
        else if (IsWhiteQueen(piece))  pieceBuf[file][rank] = 'Q';
        else if (IsWhiteRook(piece))   pieceBuf[file][rank] = 'R';
        else if (IsWhiteBishop(piece)) pieceBuf[file][rank] = 'B';
        else if (IsWhiteKnight(piece)) pieceBuf[file][rank] = 'N';
        else if (IsWhitePawn(piece))   pieceBuf[file][rank] = 'A';
        else if (IsBlackKing(piece))   pieceBuf[file][rank] = 'k';
        else if (IsBlackQueen(piece))  pieceBuf[file][rank] = 'q';
        else if (IsBlackRook(piece))   pieceBuf[file][rank] = 'r';
        else if (IsBlackBishop(piece)) pieceBuf[file][rank] = 'b';
        else if (IsBlackKnight(piece)) pieceBuf[file][rank] = 'n';
        else if (IsBlackPawn(piece))   pieceBuf[file][rank] = 'V';
        idx++;
    }

    if (m_boardState.enPassantSquare != 0ull)
    {
        uint32 file = GetFile(m_boardState.enPassantSquare);
        uint32 rank = GetRank(m_boardState.enPassantSquare);

        pieceBuf[file][rank] = '#';
    }

    if (m_boardState.lastPosMoved != 0ull)
    {
        uint32 file = GetFile(m_boardState.lastPosMoved);
        uint32 rank = GetRank(m_boardState.lastPosMoved);

        pieceBuf[file][rank] = '+';
    }

    char printBuf[1024];
    idx = 0;
    //put a lil boundry around the board
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    for (int i = 0; i < 8; i++) {
        printBuf[idx++] = (char)('a' + i);
        printBuf[idx++] = ' ';
    }
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '\n';

    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '=';
    for (int i = 0; i < 8; i++) {
        printBuf[idx++] = '=';
        printBuf[idx++] = '=';
    }
    printBuf[idx++] = '=';
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '\n';

    for (int i = 0; i < 8; i++) {
        printBuf[idx++] = '8' - i;
        printBuf[idx++] = '-';
        printBuf[idx++] = '|';
        for (int j = 0; j < 8; j++) {
            printBuf[idx++] = pieceBuf[i][j];
            printBuf[idx++] = ' ';
        }
        printBuf[idx++] = '|';
        printBuf[idx++] = '-';
        printBuf[idx++] = '8' - i;
        printBuf[idx++] = '\n';
    }

    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '=';
    for (int i = 0; i < 8; i++) {
        printBuf[idx++] = '=';
        printBuf[idx++] = '=';
    }
    printBuf[idx++] = '=';
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '\n';

    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    for (int i = 0; i < 8; i++) {
        printBuf[idx++] = (char)('a' + i);
        printBuf[idx++] = ' ';
    }
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '-';
    printBuf[idx++] = '\n';
    printBuf[idx] = '\0';

    std::cout << printBuf << std::endl;
    
    if (m_boardState.isWhiteTurn)
    {
        std::cout << "White to move" << std::endl;
    }
    else
    {
        std::cout << "Black to move" << std::endl;
    }

    bool validBoard = VerifyBoard();
    if (validBoard == false)
    {
        std::cout << "Invaild board" << std::endl;
    }
    CH_ASSERT(validBoard);
}

Piece Board::GetPieceFromPos(uint64 pos)
{
    Piece retPiece = Piece::NoPiece;
    if (IsWhite(pos))
    {
        if (IsWhiteKing(pos))        retPiece = Piece::wKing;
        else if (IsWhiteQueen(pos))  retPiece = Piece::wQueen;
        else if (IsWhiteRook(pos))   retPiece = Piece::wRook;
        else if (IsWhiteBishop(pos)) retPiece = Piece::wBishop;
        else if (IsWhiteKnight(pos)) retPiece = Piece::wKnight;
        else if (IsWhitePawn(pos))   retPiece = Piece::wPawn;
    }
    else if (IsBlack(pos))
    {
        if (IsBlackKing(pos))        retPiece = Piece::bKing;
        else if (IsBlackQueen(pos))  retPiece = Piece::bQueen;
        else if (IsBlackRook(pos))   retPiece = Piece::bRook;
        else if (IsBlackBishop(pos)) retPiece = Piece::bBishop;
        else if (IsBlackKnight(pos)) retPiece = Piece::bKnight;
        else if (IsBlackPawn(pos))   retPiece = Piece::bPawn;
    }
    return retPiece;
}

bool Board::VerifyBoard()
{
    uint32 error = 0;
    // white and black can't occupy the same square
    error |= (m_boardState.whitePieces & m_boardState.blackPieces) != 0;

    // white and black need to make up all the pieces
    error |= (m_boardState.whitePieces | m_boardState.blackPieces) != m_boardState.allPieces;

    // no pieces overlap
    uint64 whiteMask = 0ull;
    uint64 blackMask = 0ull;
    for (uint32 idx = wPawn; idx <= bKing; idx++)
    {
        error |= whiteMask & m_pieces[idx];
        error |= blackMask & m_pieces[idx];
        if (IsWhitePiece(static_cast<Piece>(idx)))
        {
            whiteMask |= m_pieces[idx];
        }
        else
        {
            CH_ASSERT(IsBlackPiece(static_cast<Piece>(idx)));
            blackMask |= m_pieces[idx];
        }
    }

    // Can't ever have more than 8 pawns
    error |= PopCount(WPawn()) > MaxPawn;
    error |= PopCount(BPawn()) > MaxPawn;

    // Can't ever not have a king
    error |= PopCount(WKing()) != 1;
    error |= PopCount(BKing()) != 1;

    // Can't ever have more than 32 pieces
    error |= PopCount(m_boardState.allPieces) > MaxPieces;
    error |= PopCount(m_boardState.whitePieces) > MaxPiecesPerSide;
    error |= PopCount(m_boardState.blackPieces) > MaxPiecesPerSide;

    // Make sure we properly reconstructed the board
    error |= m_boardState.whitePieces != whiteMask;
    error |= m_boardState.blackPieces != blackMask;

    // This has an internal assert.
    ResetZobKey();

    return error != 0;
}

//Precomputes and stores rays in all directions from all positions.
void Board::GenerateRayTable()
{
    for (uint32 dir = 0; dir < Directions::Count; dir++)
    {
        for (uint32 idx = 0; idx < 64; idx++)
        {
            uint64 pos = 1ull << idx;
            m_pRayTable[dir][idx] = GenerateRayInDirection(pos, static_cast<Directions>(dir));
        }
    }
}

template<bool isWhite>
void Board::MakeMove(const Move& move)
{
    m_boardState.previousMove = move;
    bool isCaptureOfNonPawn = (move.toPiece != Piece::NoPiece) &&
                              (move.toPiece != Piece::wPawn)   &&
                              (move.toPiece != Piece::bPawn);
    m_boardState.lastPosCaptured = (isCaptureOfNonPawn) ? move.toPos : 0ull;
    m_boardState.lastPosMoved = move.fromPos;
    // Switch the team.
    m_boardState.zobristKey ^= m_ppZobristArray[0][65];
    m_boardState.isWhiteTurn = !m_boardState.isWhiteTurn;

    // These aren't valid anymore
    m_boardState.checkAndPinMasksValid = false;
    m_boardState.illegalKingMovesValid = false;

    if (move.toPiece != Piece::NoPiece)
    {
        m_boardState.numPieceArr[move.toPiece] -= 1;

        // Subtract the value of the piece that was captured
        m_boardState.pieceValueScore -= PieceValueArray[move.toPiece];

        // %6 because we only want this going down, subtracting negative would increase it.
        m_boardState.totalMaterialValue -= PieceValueArray[move.toPiece % 6];
    }

    // Take out EP zobrist
    if (m_boardState.enPassantSquare != 0ull)
    {
        uint32 epIdx = GetIndex(m_boardState.enPassantSquare);
        m_boardState.zobristKey ^= m_ppZobristArray[Piece::NoPiece][epIdx];
    }

    if (move.flags == MoveFlags::NoFlag)
    {
        MakeNormalMove<isWhite>(move);
    }
    else if ((move.flags & MoveFlags::CastleFlags) != 0)
    {
        if constexpr (isWhite)
        {
            if (move.flags == MoveFlags::WhiteKingCastle)
            {
                MakeCastleMove<MoveFlags::WhiteKingCastle>();
            }
            else
            {
                MakeCastleMove<MoveFlags::WhiteQueenCastle>();
            }
        }
        else
        {
            if (move.flags == MoveFlags::BlackKingCastle)
            {
                MakeCastleMove<MoveFlags::BlackKingCastle>();
            }
            else
            {
                MakeCastleMove<MoveFlags::BlackQueenCastle>();
            }
        }
    }
    else if (move.flags == MoveFlags::EnPassant)
    {
        MakeEnPassantMove<isWhite>(move);
    }
    else if ((move.flags & MoveFlags::Promotion) != 0)
    {
        MakePromotionMove<isWhite>(move);
    }
    else
    {
        CH_ASSERT(false);
    }

    UpdateLastIrreversableMove<isWhite>(move);
    m_boardState.currMoveNum++;
}

template void Board::MakeMove<true>(const Move& move);
template void Board::MakeMove<false>(const Move& move);

void Board::UndoMove(BoardInfo* pBoardInfo, uint64* pPieceData)
{
    memcpy(&m_boardState, pBoardInfo, sizeof(BoardInfo));
    memcpy(&(m_pieces[0]), pPieceData, sizeof(m_pieces));
}

std::string Board::GetStringFromMove(const Move& move)
{
    std::string moveStr = "";
    moveStr += GetRank(move.fromPos) + 'a';
    moveStr += (7 - GetFile(move.fromPos)) + '1';
    moveStr += GetRank(move.toPos) + 'a';
    moveStr += (7 - GetFile(move.toPos)) + '1';

    if ((move.flags & CastleFlags) != 0)
    {
        if (move.flags == WhiteKingCastle)
        {
            moveStr = "e1g1";
        }
        else if (move.flags == WhiteQueenCastle)
        {
            moveStr = "e1c1";
        }
        else if (move.flags == BlackKingCastle)
        {
            moveStr = "e8g8";
        }
        else if (move.flags == BlackQueenCastle)
        {
            moveStr = "e8c8";
        }
        else
        {
            moveStr = "CastleFlagError";
        }
    }

    return moveStr;
}

template<bool isWhite>
void Board::MakeNullMove()
{
    m_boardState.lastPosMoved    = 0ull;
    m_boardState.lastPosCaptured = 0ull;
    // Switch the team.
    m_boardState.zobristKey ^= m_ppZobristArray[0][65];
    m_boardState.isWhiteTurn = !m_boardState.isWhiteTurn;

    // These aren't valid anymore
    m_boardState.checkAndPinMasksValid = false;
    m_boardState.illegalKingMoveMask = false;

    // Take out EP zobrist
    if (m_boardState.enPassantSquare != 0ull)
    {
        uint32 epIdx = GetIndex(m_boardState.enPassantSquare);
        m_boardState.zobristKey ^= m_ppZobristArray[Piece::NoPiece][epIdx];
    }
    m_boardState.enPassantSquare = 0ull;


    Move nullMove = {};
    nullMove.flags = 0xFF;
    UpdateLastIrreversableMove<isWhite>(nullMove);
    m_boardState.currMoveNum++;
}

template void Board::MakeNullMove<true>();
template void Board::MakeNullMove<false>();

template<bool isWhite>
void Board::MakeNormalMove(const Move& move)
{
    m_boardState.enPassantSquare = 0ull;
    m_pieces[move.fromPiece] ^= (move.toPos | move.fromPos);
    if constexpr (isWhite)
    {
        m_boardState.whitePieces ^= (move.toPos | move.fromPos);
        m_boardState.blackPieces &= ~move.toPos;
    }
    else
    {
        m_boardState.blackPieces ^= (move.toPos | move.fromPos);
        m_boardState.whitePieces &= ~move.toPos;
    }
    UpdateEnPassantSquare<isWhite>(move);
    UpdateCastleFlags<isWhite>(move);
    // the Piece::NoPiece allows this to avoid needing a branch, since
    // it is just writing to a garbage data spot
    m_pieces[move.toPiece] ^= move.toPos;

    uint32 fromIdx = GetIndex(move.fromPos);
    uint32 toIdx = GetIndex(move.toPos);

    m_boardState.zobristKey ^= m_ppZobristArray[move.fromPiece][fromIdx];
    m_boardState.zobristKey ^= m_ppZobristArray[move.fromPiece][toIdx];

    if (move.toPiece != Piece::NoPiece)
    {
        m_boardState.zobristKey ^= m_ppZobristArray[move.toPiece][toIdx];
    }

    m_boardState.allPieces = m_boardState.whitePieces | m_boardState.blackPieces;
}

template<bool isWhite>
void Board::UpdateCastleFlags(const Move& move)
{
    bool noWhiteKingside  = false;
    bool noWhiteQueenside = false;
    bool noBlackKingside  = false;
    bool noBlackQueenside = false;
    if constexpr (isWhite)
    {
        noWhiteKingside  = (move.fromPos == WhiteKingStart) || (move.fromPos == WhiteKingSideRookStart);
        noWhiteQueenside = (move.fromPos == WhiteKingStart) || (move.fromPos == WhiteQueenSideRookStart);

        noBlackKingside  = (move.toPos == BlackKingSideRookStart);
        noBlackQueenside = (move.toPos == BlackQueenSideRookStart);
    }
    else
    {
        noBlackKingside = (move.fromPos == BlackKingStart) || (move.fromPos == BlackKingSideRookStart);
        noBlackQueenside = (move.fromPos == BlackKingStart) || (move.fromPos == BlackQueenSideRookStart);

        noWhiteKingside = (move.toPos == WhiteKingSideRookStart);
        noWhiteQueenside = (move.toPos == WhiteQueenSideRookStart);
    }
    uint32 removeCastleFlags = 0;
    removeCastleFlags |= (noWhiteKingside)  ? WhiteKingCastle  : 0;
    removeCastleFlags |= (noWhiteQueenside) ? WhiteQueenCastle : 0;
    removeCastleFlags |= (noBlackKingside)  ? BlackKingCastle  : 0;
    removeCastleFlags |= (noBlackQueenside) ? BlackQueenCastle : 0;

    if (noWhiteKingside && ((m_boardState.castleMask & WhiteKingCastle) != 0))
    {
        m_boardState.zobristKey ^= m_ppZobristArray[WhiteKingCastle][65];
    }
    if (noWhiteQueenside && ((m_boardState.castleMask & WhiteQueenCastle) != 0))
    {
        m_boardState.zobristKey ^= m_ppZobristArray[WhiteQueenCastle][65];
    }
    if (noBlackKingside && ((m_boardState.castleMask & BlackKingCastle) != 0))
    {
        m_boardState.zobristKey ^= m_ppZobristArray[BlackKingCastle][65];
    }
    if (noBlackQueenside && ((m_boardState.castleMask & BlackQueenCastle) != 0))
    {
        m_boardState.zobristKey ^= m_ppZobristArray[BlackQueenCastle][65];
    }

    m_boardState.castleMask &= ~removeCastleFlags;
}

template<MoveFlags moveFlag>
void Board::MakeCastleMove()
{
    constexpr bool isWhite = ((moveFlag == WhiteKingCastle) || 
                              (moveFlag == WhiteQueenCastle));
    constexpr uint64 kingStart = (isWhite)  ? WhiteKingStart : BlackKingStart;
    constexpr uint64 kingLand  = 
        (moveFlag == WhiteKingCastle)  ? WhiteKingSideCastleLand  :
        (moveFlag == WhiteQueenCastle) ? WhiteQueenSideCastleLand :
        (moveFlag == BlackKingCastle)  ? BlackKingSideCastleLand  :
                                         BlackQueenSideCastleLand;
    constexpr uint64 rookStart = 
         (moveFlag == WhiteKingCastle)  ? WhiteKingSideRookStart  :
         (moveFlag == WhiteQueenCastle) ? WhiteQueenSideRookStart :
         (moveFlag == BlackKingCastle)  ? BlackKingSideRookStart  :
                                          BlackQueenSideRookStart;
    // rook ends on the left for kingside castle, on the right for queenside
    constexpr uint64 rookLand = 
        ((moveFlag == WhiteKingCastle) ||
         (moveFlag == BlackKingCastle)) ? MoveLeft(kingLand) : MoveRight(kingLand);

    constexpr Piece kingPiece = (isWhite) ? Piece::wKing : Piece::bKing;

    constexpr Piece rookPiece = (isWhite) ? Piece::wRook : Piece::bRook;

    m_pieces[kingPiece]  = kingLand;
    m_pieces[rookPiece] ^= (rookStart | rookLand);

    const bool legalCastle = (m_boardState.castleMask & moveFlag) != 0;
    CH_ASSERT(legalCastle);

    if constexpr (isWhite)
    {
        if ((m_boardState.castleMask & WhiteKingCastle) != 0ull)
        {
            m_boardState.zobristKey ^= m_ppZobristArray[WhiteKingCastle][65];
        }
        if ((m_boardState.castleMask & WhiteQueenCastle) != 0ull)
        {
            m_boardState.zobristKey ^= m_ppZobristArray[WhiteQueenCastle][65];
        }

        m_boardState.castleMask &= ~(WhiteKingCastle | WhiteQueenCastle);
        m_boardState.whitePieces ^= (kingStart | kingLand | rookStart | rookLand);
    }
    else
    {
        if ((m_boardState.castleMask & BlackKingCastle) != 0ull)
        {
            m_boardState.zobristKey ^= m_ppZobristArray[BlackKingCastle][65];
        }
        if ((m_boardState.castleMask & BlackQueenCastle) != 0ull)
        {
            m_boardState.zobristKey ^= m_ppZobristArray[BlackQueenCastle][65];
        }

        m_boardState.castleMask &= ~(BlackKingCastle | BlackQueenCastle);
        m_boardState.blackPieces ^= (kingStart | kingLand | rookStart | rookLand);
    }

    const int32 kingStartIdx = GetIndex(kingStart);
    const int32 kingLandIdx  = GetIndex(kingLand);
    const int32 rookStartIdx = GetIndex(rookStart);
    const int32 rookLandIdx  = GetIndex(rookLand);

    m_boardState.zobristKey ^= m_ppZobristArray[kingPiece][kingStartIdx];
    m_boardState.zobristKey ^= m_ppZobristArray[kingPiece][kingLandIdx];

    m_boardState.zobristKey ^= m_ppZobristArray[rookPiece][rookStartIdx];
    m_boardState.zobristKey ^= m_ppZobristArray[rookPiece][rookLandIdx];

    m_boardState.allPieces ^= (kingStart | kingLand | rookStart | rookLand);
    m_boardState.enPassantSquare = 0ull;
}

template<bool isWhite>
void Board::MakeEnPassantMove(const Move& move)
{
    CH_ASSERT(move.toPos == m_boardState.enPassantSquare);
    constexpr Piece teamPawn     = (isWhite) ? Piece::wPawn : 
                                               Piece::bPawn;
    constexpr Piece enemyPawn    = (isWhite) ? Piece::bPawn :
                                               Piece::wPawn;
    const uint64 enemySquare = (isWhite) ? MoveDown(move.toPos) :
                                           MoveUp(move.toPos);
    m_pieces[teamPawn]  ^= (move.fromPos | move.toPos);
    m_pieces[enemyPawn] ^= (enemySquare);

    if constexpr (isWhite)
    {
        m_boardState.whitePieces ^= move.fromPos | move.toPos;
        m_boardState.blackPieces ^= enemySquare;
    }
    else
    {
        m_boardState.blackPieces ^= move.fromPos | move.toPos;
        m_boardState.whitePieces ^= enemySquare;
    }

    const int32 fromIdx  = GetIndex(move.fromPos);
    const int32 toIdx    = GetIndex(move.toPos);
    const int32 enemyIdx = GetIndex(enemySquare);

    m_boardState.zobristKey ^= m_ppZobristArray[teamPawn][fromIdx];
    m_boardState.zobristKey ^= m_ppZobristArray[teamPawn][toIdx];
    m_boardState.zobristKey ^= m_ppZobristArray[enemyPawn][enemyIdx];

    m_boardState.allPieces ^= (move.fromPos | move.toPos | enemySquare);
    m_boardState.enPassantSquare = 0ull;
}

template<bool isWhite>
void Board::UpdateEnPassantSquare(const Move& move)
{
    if constexpr (isWhite)
    {
        if ((move.fromPiece == Piece::wPawn) &&
            (move.toPos == MoveUp(MoveUp(move.fromPos))))
        { 
            m_boardState.enPassantSquare = MoveUp(move.fromPos);
            m_boardState.zobristKey ^= m_ppZobristArray[Piece::NoPiece][GetIndex(m_boardState.enPassantSquare)];
        }
    }
    else
    {
        if ((move.fromPiece == Piece::bPawn) &&
            (move.toPos == MoveDown(MoveDown(move.fromPos))))
        {
            m_boardState.enPassantSquare = MoveDown(move.fromPos);
            m_boardState.zobristKey ^= m_ppZobristArray[Piece::NoPiece][GetIndex(m_boardState.enPassantSquare)];
        }
    }
}

template<bool isWhite>
void Board::MakePromotionMove(const Move& move)
{
    int32 fromIdx = GetIndex(move.fromPos);
    int32 toIdx   = GetIndex(move.toPos);

    Piece promotionPiece = Piece::NoPiece;
    if constexpr (isWhite)
    {
        m_pieces[Piece::wPawn] ^= move.fromPos;
        m_boardState.whitePieces ^= (move.fromPos | move.toPos);
        m_boardState.blackPieces &= (~move.toPos);

        m_boardState.zobristKey ^= m_ppZobristArray[Piece::wPawn][fromIdx];

        switch (move.flags)
        {
            case(MoveFlags::QueenPromotion):
                promotionPiece = Piece::wQueen;
                break;
            case(MoveFlags::KnightPromotion):
                promotionPiece = Piece::wKnight;
                break;
            case(MoveFlags::RookPromotion):
                promotionPiece = Piece::wRook;
                break;
            case(MoveFlags::BishopPromotion):
                promotionPiece = Piece::wBishop;
                break;
            default:
                CH_ASSERT(false);
        }
    }
    else
    {
        m_pieces[Piece::bPawn] ^= move.fromPos;
        m_boardState.blackPieces ^= (move.fromPos | move.toPos);
        m_boardState.whitePieces &= (~move.toPos);

        m_boardState.zobristKey ^= m_ppZobristArray[Piece::bPawn][fromIdx];

        switch (move.flags)
        {
            case(MoveFlags::QueenPromotion):
                promotionPiece = Piece::bQueen;
                break;
            case(MoveFlags::KnightPromotion):
                promotionPiece = Piece::bKnight;
                break;
            case(MoveFlags::RookPromotion):
                promotionPiece = Piece::bRook;
                break;
            case(MoveFlags::BishopPromotion):
                promotionPiece = Piece::bBishop;
                break;
            default:
                CH_ASSERT(false);
        }
    }

    m_pieces[promotionPiece] |= move.toPos;
    m_boardState.zobristKey ^= m_ppZobristArray[promotionPiece][toIdx];

    m_boardState.pieceValueScore -= PieceValueArray[move.fromPiece];
    m_boardState.pieceValueScore += PieceValueArray[promotionPiece];

    UpdateCastleFlags<isWhite>(move);
    m_pieces[move.toPiece] ^= move.toPos;

    if (move.toPiece != Piece::NoPiece)
    {
        m_boardState.zobristKey ^= m_ppZobristArray[move.toPiece][toIdx];
    }

    m_boardState.allPieces = m_boardState.whitePieces | m_boardState.blackPieces;
    m_boardState.enPassantSquare = 0;
}

void Board::InitZobArray()
{
    CH_ASSERT(m_ppZobristArray == nullptr);
    m_ppZobristArray = static_cast<uint64**>(malloc(sizeof(uint64*) * Piece::PieceCount));
    for (uint32 idx = 0; idx < Piece::PieceCount; idx++)
    {
        m_ppZobristArray[idx] = static_cast<uint64*>(malloc(sizeof(uint64) * 66));
    }


    constexpr uint64 randomSeed = 0x123456789abcdef;
    std::mt19937_64 gen(randomSeed);  // 64-bit Mersenne Twister engine

    // Define the range for the random integers
    std::uniform_int_distribution<uint64_t> distribution(
        std::numeric_limits<uint64_t>::min() + 1,
        std::numeric_limits<uint64_t>::max() - 1
    );

    for (uint32 i = 0; i < Piece::PieceCount; i++)
    {
        for (uint32 j = 0; j < 66; j++)
        {
            m_ppZobristArray[i][j] = distribution(gen);
        }
    }
}

void Board::ResetZobKey()
{
    uint64 key = 0ull;
    for (uint32 pieceIdx = 0; pieceIdx < Piece::NoPiece; pieceIdx++)
    {
        uint64 pieces = m_pieces[pieceIdx];

        while (pieces != 0ull)
        {
            uint64 piece = GetLSB(pieces);
            pieces ^= piece;

            uint32 pieceVal = GetIndex(piece);
            key ^= m_ppZobristArray[pieceIdx][pieceVal];
        }
    }
    if (m_boardState.enPassantSquare != 0ull)
    {
        uint32 epIdx = GetIndex(m_boardState.enPassantSquare);
        key ^= m_ppZobristArray[Piece::NoPiece][epIdx];
    }

    if (m_boardState.isWhiteTurn == false)
    {
        key ^= m_ppZobristArray[0][65];
    }
    
    if ((m_boardState.castleMask & WhiteKingCastle) != 0ull)
    {
        key ^= m_ppZobristArray[WhiteKingCastle][65];
    }
    if ((m_boardState.castleMask & WhiteQueenCastle) != 0ull)
    {
        key ^= m_ppZobristArray[WhiteQueenCastle][65];
    }
    if ((m_boardState.castleMask & BlackKingCastle) != 0ull)
    {
        key ^= m_ppZobristArray[BlackKingCastle][65];
    }
    if ((m_boardState.castleMask & BlackQueenCastle) != 0ull)
    {
        key ^= m_ppZobristArray[BlackQueenCastle][65];
    }

    if (m_boardState.zobristKey != 0ull)
    {
        CH_ASSERT(m_boardState.zobristKey == key);
    }
    m_boardState.zobristKey = key;
}

void Board::ResetPieceScore()
{
    int32 whiteMaterial = 0;
    int32 blackMaterial = 0;

    for (uint32 pieceIdx = 0; pieceIdx < Piece::PieceCount; pieceIdx++)
    {
        uint64 pieces = m_pieces[pieceIdx];
        int32 count   = PopCount(pieces);

        if (IsWhite(pieces))
        {
            if (IsWhiteKing(pieces) == false)
            {
                whiteMaterial += count * PieceValueArray[pieceIdx];
            }
        }
        else
        {
            if (IsBlackKing(pieces) == false)
            {
                blackMaterial += count * PieceValueArray[pieceIdx];
            }
        }

        m_boardState.numPieceArr[pieceIdx] = count;
    }

    // black material value is already negative here
    m_boardState.pieceValueScore    = whiteMaterial + blackMaterial;
    m_boardState.totalMaterialValue = whiteMaterial - blackMaterial;
}

// Updates last irreversable ply if our move is irreversable.  Also returns the previous ply to make
// undoing the move simpler
template<bool isWhite>
void Board::UpdateLastIrreversableMove(const Move& move)
{
    // Irreversable moves are pawn pushes, captures, and special moves (castle/promotion/ep)
    const bool isMoveIrreversable = (move.fromPiece == Piece::wPawn) ||
                                    (move.fromPiece == Piece::bPawn) ||
                                    (move.toPiece != Piece::NoPiece) ||
                                    (move.flags != 0);

    uint32 insertNum = m_boardState.currMoveNum;
    if (isWhite)
    {
        insertNum = (insertNum + 1) & ~1;
    }
    else
    {
        insertNum = (insertNum & ~1) + 1;
    }

    if (isMoveIrreversable)
    {
        m_boardState.lastIrreversableMoveNum = insertNum;
    }

    CH_ASSERT(insertNum < 1024);
    m_prevZobKeyVec[insertNum] = m_boardState.zobristKey;
}

template<bool isWhite>
bool Board::IsDrawByRepetition()
{
    int32 startIdx = m_boardState.lastIrreversableMoveNum;

    if (isWhite)
    {
        startIdx = (startIdx + 1) & ~1;
    }
    else
    {
        startIdx = (startIdx & ~1) + 1;
    }

    int32 endIdx = m_boardState.currMoveNum;

    CH_ASSERT(startIdx <= endIdx+1);

    // If it's been more than 50 moves since our last irreversable move, draw by 50 move rule
    if (endIdx - startIdx >= 50)
    {
        return true;
    }

    uint32 numRepeated = 0;
    for (uint32 idx = startIdx; idx < endIdx; idx += 2)
    {
        if (m_prevZobKeyVec[idx] == m_boardState.zobristKey)
        {
            numRepeated++;
        }
    }

    return numRepeated >= 3;
}

template bool Board::IsDrawByRepetition<true>();
template bool Board::IsDrawByRepetition<false>();

template<bool isWhite>
int32 Board::ScoreBoard()
{
    // This could be optimized to only copy/restore what's absolutley necessary...
    BoardInfo boardStateCopy = m_boardState;
    InvalidateCheckPinAndIllegalMoves();

    int32 score = m_boardState.pieceValueScore;

    uint64 whiteSliderSquares = GetSliderSeenSquares<true>(FullBoard);
    uint64 whiteMoveSquares   = GetPawnKnightKingSeenSquares<true>();
    uint64 whiteMoves         = whiteSliderSquares | whiteMoveSquares;

    uint64 blackSliderSquares = GetSliderSeenSquares<false>(FullBoard);
    uint64 blackMoveSquares   = GetPawnKnightKingSeenSquares<false>();
    uint64 blackMoves         = blackSliderSquares | blackMoveSquares;
    

    score += GetKingSafteyScore(whiteMoves, blackMoves);
    score += GetPawnBonusScores();
    score += GetRookBonusScores();
    score += GetKnightBonusScores();

    int32 whiteMobilityScore = GeneralMobilityScore * (PopCount(GetWhitePieces() & whiteMoves));
    int32 blackMobilityScore = GeneralMobilityScore * (PopCount(GetBlackPieces() & blackMoves));

    score += (whiteMobilityScore - blackMobilityScore);

    constexpr int32 multFactor = (isWhite) ? 1 : -1;
    score *= multFactor;


    // Without queens rooks or pawns, this is probably a drawn game
    const bool probablyDraw = (GetQueen<true>() | GetQueen<false>() |
                               GetRook<true>()  | GetRook<false>()  |
                               GetPawn<true>()  | GetPawn<false>()) == 0ull;
    if (probablyDraw)
    {
        score /= 10;
    }

    m_boardState = boardStateCopy;

    // drop the low 4 bits, helps TT
    score &= ~0xF;
    return score;
}

int32 Board::GetKingSafteyScore(uint64 whiteSeenSquares, uint64 blackSeenSquares)
{
    // white king
    int32 whiteKingScore = 0;
    uint64 whiteKingPos = (GetKing<true>() & (WhiteKingSideCastleLand | WhiteQueenSideCastleLand));

    uint64 whiteKingMoves = GetKingMoves<true, true>(whiteKingPos);

    uint64 whiteKingTouchingPawns = whiteKingMoves & GetPawn<true>();
    whiteKingScore += PawnOneAwayFromCastledKing * PopCount(whiteKingTouchingPawns);
    whiteKingScore += PawnTwoAwayFromCastledKing * PopCount(MoveUp(whiteKingMoves) & ~whiteKingTouchingPawns);
    whiteKingScore += CutoffKingMoveScore        * PopCount(whiteKingMoves & blackSeenSquares);
    whiteKingScore += NormalPieceTouchingKing    * PopCount(whiteKingMoves & GetWhitePieces());


    // black king
    int32 blackKingScore = 0;
    uint64 blackKingPos = (GetKing<false>() & (BlackKingSideCastleLand | BlackQueenSideCastleLand));

    uint64 blackKingMoves = GetKingMoves<false, true>(blackKingPos);

    uint64 blackKingTouchingPawns = blackKingMoves & GetPawn<false>();
    blackKingScore += PawnOneAwayFromCastledKing * PopCount(blackKingTouchingPawns);
    blackKingScore += PawnTwoAwayFromCastledKing * PopCount(MoveUp(blackKingMoves) & ~blackKingTouchingPawns);
    blackKingScore += CutoffKingMoveScore        * PopCount(blackKingMoves & whiteSeenSquares);
    blackKingScore += NormalPieceTouchingKing    * PopCount(blackKingMoves & GetWhitePieces());

    return whiteKingScore - blackKingScore;
}

int32 Board::GetPawnBonusScores()
{
    int32 endMultiplier   = 1;
    int32 earlyMultiplier = 2;
    // This might need to be tweaked, but it should be an threshold
    const bool isEndGame = m_boardState.totalMaterialValue < ((QueenScore + RookScore) * 2);
    if (isEndGame)
    {
        endMultiplier   = 2;
        earlyMultiplier = 1;
    }

    int32 whiteScore = 0;
    uint64 whitePawns = GetPawn<true>();
    uint64 whitePawnsDefendingPawns = (MoveUpRight(whitePawns) | (MoveUpLeft(whitePawns))) & whitePawns;
    uint64 whiteDoubledPawns        = MoveUp(whitePawns) & whitePawns;

    whiteScore += PawnChainScore   * PopCount(whitePawnsDefendingPawns);
    whiteScore += DoubledPawnScore * PopCount(whiteDoubledPawns);

    int32 blackScore = 0;
    uint64 blackPawns = GetPawn<false>();
    uint64 blackPawnsDefendingPawns = (MoveUpRight(blackPawns) | (MoveUpLeft(blackPawns))) & blackPawns;
    uint64 blackDoubledPawns        = MoveUp(blackPawns) & blackPawns;

    blackScore += PawnChainScore   * PopCount(blackPawnsDefendingPawns);
    blackScore += DoubledPawnScore * PopCount(blackDoubledPawns);

    whiteScore *= earlyMultiplier;
    blackScore *= earlyMultiplier;

    uint64 whitePawnKillMask = whitePawns | MoveLeft(whitePawns) | MoveRight(whitePawns);
    uint64 blackPawnKillMask = blackPawns | MoveLeft(blackPawns) | MoveRight(blackPawns);

    uint64 whitePassedPawns = whitePawns;
    uint64 blackPassedPawns = blackPawns;

    for (uint32 idx = 0; idx < 5; idx++)
    {
        whitePassedPawns = MoveUp(whitePassedPawns)   & ~blackPawnKillMask;
        blackPassedPawns = MoveDown(blackPassedPawns) & ~whitePawnKillMask;
    }


    int32 whiteEndScore = 0;
    int32 blackEndScore = 0;
    if (whitePassedPawns != 0ull)
    {
        whiteEndScore += FarPassedPawnScore   * PopCount(whitePassedPawns & (Rank2 | Rank3));
        whiteEndScore += MidPassedPawnScore   * PopCount(whitePassedPawns & (Rank4 | Rank5));
        whiteEndScore += ClosePassedPawnScore * PopCount(whitePassedPawns & (Rank6 | Rank7));

    }
    if (blackPassedPawns != 0ull)
    {
        blackEndScore += FarPassedPawnScore   * PopCount(blackPassedPawns & (Rank7 | Rank6));
        blackEndScore += MidPassedPawnScore   * PopCount(blackPassedPawns & (Rank5 | Rank4));
        blackEndScore += ClosePassedPawnScore * PopCount(blackPassedPawns & (Rank3 | Rank2));
    }

    whiteEndScore += FarPawnAdvanceScore   * PopCount(whitePawns & (Rank2 | Rank3));
    whiteEndScore += MidPawnAdvanceScore   * PopCount(whitePawns & (Rank4 | Rank5));
    whiteEndScore += ClosePawnAdvanceScore * PopCount(whitePawns & (Rank6 | Rank7));

    blackEndScore += FarPawnAdvanceScore   * PopCount(blackPawns & (Rank7 | Rank6));
    blackEndScore += MidPawnAdvanceScore   * PopCount(blackPawns & (Rank5 | Rank4));
    blackEndScore += ClosePawnAdvanceScore * PopCount(blackPawns & (Rank3 | Rank2));

    whiteEndScore *= endMultiplier;
    blackEndScore *= endMultiplier;

    whiteScore += whiteEndScore;
    blackScore += blackEndScore;

    return whiteScore - blackScore;
}


// I should add open files
int32 Board::GetRookBonusScores()
{
    uint32 numWhitePawns = m_boardState.numPieceArr[wPawn];
    uint32 numBlackPawns = m_boardState.numPieceArr[bPawn];
    int32 whiteScore = m_boardState.numPieceArr[wRook] * RookAdjustmentScores[numWhitePawns];
    int32 blackScore = m_boardState.numPieceArr[bRook] * RookAdjustmentScores[numBlackPawns];

    return whiteScore - blackScore;
}

int32 Board::GetKnightBonusScores()
{
    uint32 numWhitePawns = m_boardState.numPieceArr[wPawn];
    uint32 numBlackPawns = m_boardState.numPieceArr[bPawn];
    int32 whiteScore = m_boardState.numPieceArr[wKnight] * KnightAdjustmentScores[numWhitePawns];
    int32 blackScore = m_boardState.numPieceArr[bKnight] * KnightAdjustmentScores[numBlackPawns];

    return whiteScore - blackScore;
}

// This will give extra points for being aggressive with our king in the endgame
int32 Board::AggressiveKingEndgameBonus()
{
    int32 whiteBonus = 0;
    int32 blackBonus = 0;
    bool isEndGame    = m_boardState.totalMaterialValue < QueenScore + KnightScore;
    bool whiteWinning = m_boardState.pieceValueScore > 0;
    bool blackWinning = m_boardState.pieceValueScore < 0;
    if (isEndGame && whiteWinning)
    {
        int32 whiteKingFile = GetFile(GetKing<true>());
        int32 whiteKingRank = GetRank(GetKing<true>());
        int32 blackKingFile = GetFile(GetKing<true>());
        int32 blackKingRank = GetRank(GetKing<true>());

        int32 fileDiff = 8 - std::abs(whiteKingFile - blackKingFile);
        int32 rankDiff = 8 - std::abs(whiteKingRank - blackKingRank);

        int32 whiteBonus = AggressiveKingEndgameScore * (fileDiff + rankDiff);
    }
    else if (isEndGame && blackWinning)
    {
        int32 whiteKingFile = GetFile(GetKing<true>());
        int32 whiteKingRank = GetRank(GetKing<true>());
        int32 blackKingFile = GetFile(GetKing<true>());
        int32 blackKingRank = GetRank(GetKing<true>());

        int32 fileDiff = 8 - std::abs(whiteKingFile - blackKingFile);
        int32 rankDiff = 8 - std::abs(whiteKingRank - blackKingRank);

        int32 blackBonus = AggressiveKingEndgameScore * (fileDiff + rankDiff);
    }

    return whiteBonus - blackBonus;
}

template int32 Board::ScoreBoard<true>();
template int32 Board::ScoreBoard<false>();