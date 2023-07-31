#pragma once
#include "util.h"
#include <string>

enum Piece : uint32
{
    wKing = 0,
    wQueen = 1,
    wRook = 2,
    wKnight = 3,
    wBishop = 4,
    wPawn = 5,

    bKing = 6,
    bQueen = 7,
    bRook = 8,
    bKnight = 9,
    bBishop = 10,
    bPawn = 11,

    NoPiece = 12,

    PieceCount = 13,
};

static constexpr uint32 MaxPieces = 32;
static constexpr uint32 MaxPiecesPerSide = 16;
static constexpr uint32 MaxPawn = 8;
static constexpr bool IsWhitePiece(Piece piece) { return piece <= Piece::wPawn; }
static constexpr bool IsBlackPiece(Piece piece) { return !IsWhitePiece(piece); }

enum class Turn : uint32
{
    White = 0,
    Black = 1,
};

static constexpr uint64 WhiteKingStart           = 0x0000000000000010ull;
static constexpr uint64 BlackKingStart           = 0x1000000000000000ull;

static constexpr uint64 WhiteKingSideCastleLand  = 0x0000000000000040ull;
static constexpr uint64 WhiteQueenSideCastleLand = 0x0000000000000004ull;
static constexpr uint64 BlackKingSideCastleLand  = 0x4000000000000000ull;
static constexpr uint64 BlackQueenSideCastleLand = 0x0400000000000000ull;

static constexpr uint64 WhiteKingSideRookStart   = 0x0000000000000080ull;
static constexpr uint64 WhiteQueenSideRookStart  = 0x0000000000000001ull;
static constexpr uint64 BlackKingSideRookStart   = 0x8000000000000000ull;
static constexpr uint64 BlackQueenSideRookStart  = 0x0100000000000000ull;

enum MoveFlags : uint32
{
    NoFlag           = 0,
    WhiteKingCastle  = 1 << 0,
    WhiteQueenCastle = 1 << 1,
    BlackKingCastle  = 1 << 2,
    BlackQueenCastle = 1 << 3,

    CastleFlags      = WhiteKingCastle  | 
                       WhiteQueenCastle | 
                       BlackKingCastle  |
                       BlackQueenCastle,

    EnPassant        = 1 << 4,

    BishopPromotion  = 1 << 5,
    KnightPromotion  = 1 << 6,
    RookPromotion    = 1 << 7,
    QueenPromotion   = 1 << 8,

    Promotion        = BishopPromotion |
                       KnightPromotion |
                       RookPromotion   |
                       QueenPromotion,
};

struct Move
{
    uint64 fromPos;
    uint64 toPos;
    Piece  fromPiece;
    Piece  toPiece;
    uint32 flags;
};

class Board
{
public:
    Board();
    ~Board();

    Result Init();
    Result Destroy();

    void PrintBoard();
    void ResetBoard();

    Piece GetPieceFromPos(uint64 pos);

    template<bool isWhite>
    void MakeMove(const Move& move);

    bool VerifyBoard();

    uint64 GetEnPassantPos() { return m_enPassantSquare; }

    void GenerateLegalMoves(Move* moveList, uint32* numMoves);
private:
    template<bool isWhite>
    void MakeNormalMove(const Move& move);

    template<bool isWhite>
    void MakeEnPassantMove(const Move& move);

    template<MoveFlags moveFlag>
    void MakeCastleMove();

    template<bool isWhite>
    void MakePromotionMove(const Move& move);

    template<bool IsWhite>
    void UpdateEnPassantSquare(const Move& move);

    inline  uint64 WKing()   { return m_pieces[Piece::wKing];   }
    inline  uint64 WQueen()  { return m_pieces[Piece::wQueen];  }
    inline  uint64 WRook()   { return m_pieces[Piece::wRook];   }
    inline  uint64 WBishop() { return m_pieces[Piece::wBishop]; }
    inline  uint64 WKnight() { return m_pieces[Piece::wKnight]; }
    inline  uint64 WPawn()   { return m_pieces[Piece::wPawn];   }
    
    inline  uint64 BKing()   { return m_pieces[Piece::bKing];   }
    inline  uint64 BQueen()  { return m_pieces[Piece::bQueen];  }
    inline  uint64 BRook()   { return m_pieces[Piece::bRook];   }
    inline  uint64 BBishop() { return m_pieces[Piece::bBishop]; }
    inline  uint64 BKnight() { return m_pieces[Piece::bKnight]; }
    inline  uint64 BPawn()   { return m_pieces[Piece::bPawn];   }

    inline  bool IsWhiteKing  (uint64 piece) { return (piece & WKing())   != 0; }
    inline  bool IsWhiteQueen (uint64 piece) { return (piece & WQueen())  != 0; }
    inline  bool IsWhiteRook  (uint64 piece) { return (piece & WRook())   != 0; }
    inline  bool IsWhiteBishop(uint64 piece) { return (piece & WBishop()) != 0; }
    inline  bool IsWhiteKnight(uint64 piece) { return (piece & WKnight()) != 0; }
    inline  bool IsWhitePawn  (uint64 piece) { return (piece & WPawn())   != 0; }

    inline  bool IsBlackKing(uint64 piece)   { return (piece & BKing())   != 0; }
    inline  bool IsBlackQueen(uint64 piece)  { return (piece & BQueen())  != 0; }
    inline  bool IsBlackRook(uint64 piece)   { return (piece & BRook())   != 0; }
    inline  bool IsBlackBishop(uint64 piece) { return (piece & BBishop()) != 0; }
    inline  bool IsBlackKnight(uint64 piece) { return (piece & BKnight()) != 0; }
    inline  bool IsBlackPawn(uint64 piece)   { return (piece & BPawn())   != 0; }

    inline  bool IsBlack(uint64 piece) { return (m_blackPieces & piece) != 0; }
    inline  bool IsWhite(uint64 piece) { return (m_whitePieces & piece) != 0; }

    void SetBoardFromFEN(std::string fen);

    void GenerateRayTable();

    uint64 m_pieces[static_cast<uint32>(Piece::PieceCount) + 1];
    uint64 m_blackPieces;
    uint64 m_whitePieces;
    uint64 m_allPieces;
    uint32 m_castleMask;

    uint64 m_enPassantSquare;

    uint64 m_rayTable[Directions::Count][64];

    Turn   m_turn;
};