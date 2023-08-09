#include "../inc/board.h"
#include "../inc/util.h"
#include "../inc/bitHelper.h"
#include <random>
Board::Board()
:
m_pieces(),
m_boardState(),
m_pRayTable(),
m_ppZobristArray(nullptr)
{

}

Board::~Board()
{

}

Result Board::Init()
{
    InitZobArray();
    ResetBoard();
    GenerateRayTable();

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
        else if (IsWhitePawn(piece))   pieceBuf[file][rank] = 'P';
        else if (IsBlackKing(piece))   pieceBuf[file][rank] = 'k';
        else if (IsBlackQueen(piece))  pieceBuf[file][rank] = 'q';
        else if (IsBlackRook(piece))   pieceBuf[file][rank] = 'r';
        else if (IsBlackBishop(piece)) pieceBuf[file][rank] = 'b';
        else if (IsBlackKnight(piece)) pieceBuf[file][rank] = 'n';
        else if (IsBlackPawn(piece))   pieceBuf[file][rank] = 'p';
        idx++;
    }

    if (m_boardState.enPassantSquare != 0ull)
    {
        uint32 file = GetFile(m_boardState.enPassantSquare);
        uint32 rank = GetRank(m_boardState.enPassantSquare);

        pieceBuf[file][rank] = '#';
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

    bool validBoard = VerifyBoard();
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
#if VERIFY_BOARD
    bool validBoard = VerifyBoard();
    CH_ASSERT(validBoard == true);
#endif
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

    if constexpr (isWhite)
    {
        m_pieces[Piece::wPawn] ^= move.fromPos;
        m_boardState.whitePieces ^= (move.fromPos | move.toPos);
        m_boardState.blackPieces &= (~move.toPos);

        m_boardState.zobristKey ^= m_ppZobristArray[Piece::wPawn][fromIdx];

        switch (move.flags)
        {
            case(MoveFlags::QueenPromotion):
                m_pieces[Piece::wQueen]  |= move.toPos;
                m_boardState.zobristKey ^= m_ppZobristArray[Piece::wQueen][toIdx];
                break;
            case(MoveFlags::KnightPromotion):
                m_pieces[Piece::wKnight] |= move.toPos;
                m_boardState.zobristKey ^= m_ppZobristArray[Piece::wKnight][toIdx];
                break;
            case(MoveFlags::RookPromotion):
                m_pieces[Piece::wRook]   |= move.toPos;
                m_boardState.zobristKey ^= m_ppZobristArray[Piece::wRook][toIdx];
                break;
            case(MoveFlags::BishopPromotion):
                m_pieces[Piece::wBishop] |= move.toPos;
                m_boardState.zobristKey ^= m_ppZobristArray[Piece::wBishop][toIdx];
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
                m_pieces[Piece::bQueen]  |= move.toPos;
                m_boardState.zobristKey ^= m_ppZobristArray[Piece::bQueen][toIdx];
                break;
            case(MoveFlags::KnightPromotion):
                m_pieces[Piece::bKnight] |= move.toPos;
                m_boardState.zobristKey ^= m_ppZobristArray[Piece::bKnight][toIdx];
                break;
            case(MoveFlags::RookPromotion):
                m_pieces[Piece::bRook]   |= move.toPos;
                m_boardState.zobristKey ^= m_ppZobristArray[Piece::bRook][toIdx];
                break;
            case(MoveFlags::BishopPromotion):
                m_pieces[Piece::bBishop] |= move.toPos;
                m_boardState.zobristKey ^= m_ppZobristArray[Piece::bBishop][toIdx];
                break;
            default:
                CH_ASSERT(false);
        }
    }

    UpdateCastleFlags<isWhite>(move);
    m_pieces[move.toPiece] ^= move.toPos;

    if (move.toPiece != Piece::NoPiece)
    {
        m_boardState.zobristKey ^= m_ppZobristArray[move.toPiece][toIdx];
    }

    m_boardState.allPieces = m_boardState.whitePieces | m_boardState.blackPieces;
    m_boardState.enPassantSquare = 0;
}

int32 Board::ScoreBoard()
{
    int32 score = 0;

    score += QueenScore  * (PopCount(WQueen())  - PopCount(BQueen()));
    score += RookScore   * (PopCount(WRook())   - PopCount(BRook()));
    score += BishopScore * (PopCount(WBishop()) - PopCount(BBishop()));
    score += KnightScore * (PopCount(WKnight()) - PopCount(BKnight()));
    score += PawnScore   * (PopCount(WPawn())   - PopCount(BPawn()));
    
    return score;
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