#include "../inc/board.h"
#include "../inc/util.h"
#include "../inc/bitHelper.h"

Board::Board()
:
m_pieces(),
m_castleMask(0),
m_turn(Turn::White),
m_allPieces(0),
m_whitePieces(0),
m_blackPieces(0),
m_enPassantSquare(0)
{

}

Board::~Board()
{

}

Result Board::Init()
{
    ResetBoard();
    GenerateRayTable();
    return Result::ErrorNotImplemented;
}

Result Board::Destroy()
{
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
                fenStrIdx++;
                // don't want to increment boardIdx again
                continue;
        } // switch (fenStr[fenStrIdx])
        fenStrIdx++;
        boardIdx++;
    }

    while (fenStr[fenStrIdx] == ' ')
    {
        fenStrIdx++;
    }
    m_turn = Turn::Black;
    if (fenStr[fenStrIdx] == 'w')
    {
        m_turn = Turn::White;
    }

    fenStrIdx++;
    fenStrIdx++;

    // handle castling
    m_castleMask = 0;
    while ((fenStr[fenStrIdx] != ' ') && (fenStr[fenStrIdx] != '-'))
    {
        if (fenStr[fenStrIdx] == 'K')
        {
            m_castleMask |= MoveFlags::WhiteKingCastle;
        }
        else if (fenStr[fenStrIdx] == 'Q')
        {
            m_castleMask |= MoveFlags::WhiteQueenCastle;
        }
        else if (fenStr[fenStrIdx] == 'k')
        {
            m_castleMask |= MoveFlags::BlackKingCastle;
        }
        else if (fenStr[fenStrIdx] == 'q')
        {
            m_castleMask |= MoveFlags::BlackQueenCastle;
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
    m_whitePieces = 0ull;
    m_blackPieces = 0ull;
    for (uint32 idx = 0; idx < Piece::PieceCount; idx++)
    {
        if (IsWhitePiece(static_cast<Piece>(idx)))
        {
            m_whitePieces |= m_pieces[idx];
        }
        else
        {
            CH_ASSERT(IsBlackPiece(static_cast<Piece>(idx)));
            m_blackPieces |= m_pieces[idx];
        }
    }
    m_allPieces = m_whitePieces | m_blackPieces;
}

void Board::ResetBoard()
{
    std::string fenStr = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
    SetBoardFromFEN(fenStr);
}

void Board::PrintBoard()
{
    char pieceBuf[8][8];
    for (uint32 x = 0; x < 8; x++)
    {
        for (uint32 y = 0; y < 8; y++)
        {
            pieceBuf[x][y] = '.';
        }
    }

    uint64 pieceList[MaxPieces + 1] = {};
    GetIndividualBits(m_allPieces, pieceList);
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
        else CH_ASSERT(false);
        idx++;
    }

    if (m_enPassantSquare != 0ull)
    {
        uint32 file = GetFile(m_enPassantSquare);
        uint32 rank = GetRank(m_enPassantSquare);

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
    error |= (m_whitePieces & m_blackPieces) != 0;

    // white and black need to make up all the pieces
    error |= (m_whitePieces | m_blackPieces) != m_allPieces;

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
    error |= PopCount(m_allPieces) > MaxPieces;
    error |= PopCount(m_whitePieces) > MaxPiecesPerSide;
    error |= PopCount(m_blackPieces) > MaxPiecesPerSide;

    // Make sure we properly reconstructed the board
    error |= m_whitePieces != whiteMask;
    error |= m_blackPieces != blackMask;

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
            m_rayTable[dir][idx] = GenerateRayInDirection(pos, static_cast<Directions>(dir));
        }
    }
}

template<bool isWhite>
void Board::MakeMove(const Move& move)
{
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

template<bool isWhite>
void Board::MakeNormalMove(const Move& move)
{
    m_enPassantSquare = 0ull;
    m_pieces[move.fromPiece] ^= (move.toPos | move.fromPos);
    if constexpr (isWhite)
    {
        m_whitePieces ^= (move.toPos | move.fromPos);
        m_blackPieces &= ~move.toPos;

        UpdateEnPassantSquare<true>(move);
    }
    else
    {
        CH_ASSERT(IsBlackPiece(move.fromPiece));
        m_blackPieces ^= (move.toPos | move.fromPos);
        m_whitePieces &= ~move.toPos;

        UpdateEnPassantSquare<false>(move);
    }

    // the Piece::NoPiece allows this to avoid needing a branch, since
    // it is just writing to a garbage data spot
    m_pieces[move.toPiece] ^= move.toPos;

    m_allPieces = m_whitePieces | m_blackPieces;
}

template void Board::MakeNormalMove<true>(const Move& move);
template void Board::MakeNormalMove<false>(const Move& move);

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

    const bool legalCastle = (m_castleMask & moveFlag) != 0;
    CH_ASSERT(legalCastle);

    if constexpr (isWhite)
    {
        CH_ASSERT(m_turn == Turn::White);
        m_castleMask &= ~(WhiteKingCastle | WhiteQueenCastle);
        m_whitePieces ^= (kingStart | kingLand | rookStart | rookLand);
    }
    else
    {
        CH_ASSERT(m_turn == Turn::Black);
        m_castleMask &= ~(BlackKingCastle | BlackQueenCastle);
        m_blackPieces ^= (kingStart | kingLand | rookStart | rookLand);
    }
    m_allPieces ^= (kingStart | kingLand | rookStart | rookLand);
    m_enPassantSquare = 0ull;
}

template void Board::MakeCastleMove<MoveFlags::WhiteKingCastle> ();
template void Board::MakeCastleMove<MoveFlags::WhiteQueenCastle>();
template void Board::MakeCastleMove<MoveFlags::BlackKingCastle> ();
template void Board::MakeCastleMove<MoveFlags::BlackQueenCastle>();

template<bool isWhite>
void Board::MakeEnPassantMove(const Move& move)
{
    CH_ASSERT(move.toPos == m_enPassantSquare);
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

        m_whitePieces ^= move.fromPos | move.toPos;
        m_blackPieces ^= enemySquare;
    }
    else
    {
        m_blackPieces ^= move.fromPos | move.toPos;
        m_whitePieces ^= enemySquare;
    }
    m_allPieces ^= (move.fromPos | move.toPos | enemySquare);
    m_enPassantSquare = 0ull;
}

template void Board::MakeEnPassantMove<true> (const Move& move);
template void Board::MakeEnPassantMove<false>(const Move& move);

template<bool isWhite>
void Board::UpdateEnPassantSquare(const Move& move)
{
    if constexpr (isWhite)
    {
        if ((move.fromPiece == Piece::wPawn) &&
            (move.toPos == MoveUp(MoveUp(move.fromPos))))
        { 
            m_enPassantSquare = MoveUp(move.fromPos);
        }
    }
    else
    {
        if ((move.fromPiece == Piece::bPawn) &&
            (move.toPos == MoveDown(MoveDown(move.fromPos))))
        {
            m_enPassantSquare = MoveDown(move.fromPos);
        }
    }
}

template void Board::UpdateEnPassantSquare<true>(const Move& move);
template void Board::UpdateEnPassantSquare<false>(const Move& move);

template<bool isWhite>
void Board::MakePromotionMove(const Move& move)
{
    if constexpr (isWhite)
    {
        m_pieces[Piece::wPawn] ^= move.fromPos;
        m_whitePieces ^= (move.fromPos | move.toPos);
        m_blackPieces &= (~move.toPos);

        switch (move.flags)
        {
            case(MoveFlags::QueenPromotion):
                m_pieces[Piece::wQueen]  |= move.toPos;
                break;
            case(MoveFlags::KnightPromotion):
                m_pieces[Piece::wKnight] |= move.toPos;
                break;
            case(MoveFlags::RookPromotion):
                m_pieces[Piece::wRook]   |= move.toPos;
                break;
            case(MoveFlags::BishopPromotion):
                m_pieces[Piece::wBishop] |= move.toPos;
                break;
            default:
                CH_ASSERT(false);
        }
    }
    else
    {
        m_pieces[Piece::bPawn] ^= move.fromPos;
        m_blackPieces ^= (move.fromPos | move.toPos);
        m_whitePieces &= (~move.toPos);

        switch (move.flags)
        {
            case(MoveFlags::QueenPromotion):
                m_pieces[Piece::bQueen]  |= move.toPos;
                break;
            case(MoveFlags::KnightPromotion):
                m_pieces[Piece::bKnight] |= move.toPos;
                break;
            case(MoveFlags::RookPromotion):
                m_pieces[Piece::bRook]   |= move.toPos;
                break;
            case(MoveFlags::BishopPromotion):
                m_pieces[Piece::bBishop] |= move.toPos;
                break;
            default:
                CH_ASSERT(false);
        }
    }
    m_pieces[move.toPiece] ^= move.toPos;

    m_allPieces = m_whitePieces | m_blackPieces;
    m_enPassantSquare = 0;
}

template void Board::MakePromotionMove<true>(const Move& move);
template void Board::MakePromotionMove<false>(const Move& move);

void Board::GenerateLegalMoves(Move* moveList, uint32* numMoves)
{

}