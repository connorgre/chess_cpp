#pragma once
#include "util.h"
#include <string>
#include <vector>

enum Piece : uint32
{
    wKing         = 0,
    wQueen        = 1,
    wRook         = 2,
    wKnight       = 3,
    wBishop       = 4,
    wPawn         = 5,

    bKing         = 6,
    bQueen        = 7,
    bRook         = 8,
    bKnight       = 9,
    bBishop       = 10,
    bPawn         = 11,

    NoPiece       = 12,

    PieceCount    = 13,
    EndOfMoveList = 14,
};

enum PieceScores : int32
{
    KingScore   = 0x0FFF,
    QueenScore  = 900,
    RookScore   = 500,
    BishopScore = 320,
    KnightScore = 300,
    PawnScore   = 100,

    PawnOneAwayFromCastledKing =  30,
    PawnTwoAwayFromCastledKing =  15,
    NormalPieceTouchingKing    =   5,
    CutoffKingMoveScore        = -15,

    PawnChainScore        =  10,
    DoubledPawnScore      = -70,
    FarPassedPawnScore    =  PawnScore / 2,     // extra points for passed pawns
    MidPassedPawnScore    =  PawnScore,
    ClosePassedPawnScore  =  PawnScore * 2,
    FarPawnAdvanceScore   =  -5,            // Lose points for unpushed pawns
    MidPawnAdvanceScore   =   5,
    ClosePawnAdvanceScore =  20,

    GeneralMobilityScore  =   1,            // Points for having legal moves
};

                                      // white scores
constexpr int32 PieceValueArray[] = { PieceScores::KingScore,
                                      PieceScores::QueenScore,
                                      PieceScores::RookScore,
                                      PieceScores::KnightScore,
                                      PieceScores::BishopScore,
                                      PieceScores::PawnScore,
                                      // black scores (whitescores * -1).
                                 -1 * PieceScores::KingScore,
                                 -1 * PieceScores::QueenScore,
                                 -1 * PieceScores::RookScore,
                                 -1 * PieceScores::KnightScore,
                                 -1 * PieceScores::BishopScore,
                                 -1 * PieceScores::PawnScore,
                                      //NoMove score
                                      0 };

// rooks gain more value with less pawns, knights lose value with less pawns
constexpr int32 RookAdjustmentScores[]   = { 15,  12,  9, 6, 3, 0, -3, -6, -9};
constexpr int32 KnightAdjustmentScores[] = {-15, -10, -5, 0, 4, 8, 12,  15, 20};

// This will be passed pawn pushes, promotions, castles, and maybe some other stuff.
constexpr uint32 MaxNumProbablyGoodMoves = 16;
enum MoveTypes : uint32
{
    Best             = 0,
    ProbablyGood     = 1,
    Attack           = 2,
    Killer           = 3,
    Normal           = 4,

    MoveTypeCount,
};

static constexpr uint32 MaxPieces           = 32;
static constexpr uint32 MaxPiecesPerSide    = 16;
static constexpr uint32 MaxPawn             = 8;
static constexpr uint32 MaxMovesPerPosition = 192;  // I think it is technically something like 219

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
    int32  score;
};

struct BoardInfo
{
    uint64 blackPieces;
    uint64 whitePieces;
    uint64 allPieces;
    uint32 castleMask;

    uint64 enPassantSquare;

    bool isWhiteTurn;

    // Squares that are legal to move to to get out of check.  If we are not in check, this is a
    // mask of the whole board.
    uint64 checkMask;

    // pinmask for horizontal/vertical moves.  If a piece is in the pinmask, they can only move to
    // another piece in the pinmask
    uint64 hvPinMask;

    // used to help check the legality of en passant moves.  En passant is the only move where 2
    // pieces can disappear from a rank at the same time, so to ensure legality of en passant, we
    // need to make sure that our pawn and the enemy pawn don't make up this mask
    uint64 doubleHorizontalPinMask;

    // pinmask for diagonal moves.
    uint64 diagPinMask;

    // If the king is checked by a sliding piece, the square behind the king is put into this mask.
    uint64 kingXRayMoveMask;

    // A mask of squares the king is not allowed to move to
    uint64 illegalKingMoveMask;

    // The number of pieces putting the king in check.  This is used to determine if we are in
    // check by two pieces at once, in which case we can only move our king
    uint32 numPiecesChecking;

    uint32 legalCastles;

    // Hash of the board into the transposition table.
    uint64 zobristKey;

    bool checkAndPinMasksValid;

    bool illegalKingMovesValid;

    int32 pieceValueScore;      // whiteMaterial - blackMaterial

    uint64 lastPosMoved;
    uint64 lastPosCaptured;

    uint8 numPieceArr[Piece::PieceCount];
    int32 totalMaterialValue;

    uint32 lastIrreversableMoveNum;

    uint32 currMoveNum;
};

constexpr uint32 boardInfosize = sizeof(BoardInfo);

class Board
{
public:
    Board();
    ~Board();

    Result Init();
    Result Destroy();

    void PrintBoard(uint64 pieces);
    void ResetBoard();

    Piece GetPieceFromPos(uint64 pos);

    template<bool isWhite>
    void MakeMove(const Move& move);

    template<bool isWhite>
    void MakeNullMove();

    void UndoMove(BoardInfo* pBoardInfo, uint64* pPieceData);

    bool VerifyBoard();

    uint64 GetEnPassantPos() { return m_boardState.enPassantSquare; }

    template<bool isWhite>
    void GenerateCheckAndPinMask();

    template<bool isWhite>
    void GenerateIllegalKingMoveMask();

    // Gets all the legal moves and puts them in a pre-allocated list of moves.  The final move
    // in each list has Move::fromPiece == EndOfMoveList
    template<bool isWhite, bool onlyCaptures>
    void GenerateLegalMoves(Move** ppMoveList, uint32* pNumMoves = nullptr);

    void CopyBoardData(BoardInfo* pBoardInfo) { memcpy(pBoardInfo, &m_boardState, sizeof(BoardInfo)); }
    void CopyPieceData(uint64* pPieceData) { memcpy(pPieceData, &(m_pieces[0]), sizeof(m_pieces)); }

    template<Piece piece, bool isWhite>
    inline uint64 GetPieces()
    {
        if constexpr (piece == wKing)        { return GetKing<isWhite>();   }
        else if constexpr (piece == wQueen)  { return GetQueen<isWhite>();  }
        else if constexpr (piece == wRook)   { return GetRook<isWhite>();   }
        else if constexpr (piece == wBishop) { return GetBishop<isWhite>(); }
        else if constexpr (piece == wKnight) { return GetKnight<isWhite>(); }
        else if constexpr (piece == wPawn)   { return GetPawn<isWhite>();   }
        else static_assert(false);
    }

    template<bool isWhite>
    inline uint64 GetKing()   { if constexpr (isWhite) return WKing();   else return BKing();   }
    template<bool isWhite>
    inline uint64 GetQueen()  { if constexpr (isWhite) return WQueen();  else return BQueen();  }
    template<bool isWhite>
    inline uint64 GetRook()   { if constexpr (isWhite) return WRook();   else return BRook();   }
    template<bool isWhite>
    inline uint64 GetBishop() { if constexpr (isWhite) return WBishop(); else return BBishop(); }
    template<bool isWhite>
    inline uint64 GetKnight() { if constexpr (isWhite) return WKnight(); else return BKnight(); }
    template<bool isWhite>
    inline uint64 GetPawn()   { if constexpr (isWhite) return WPawn();   else return BPawn();   }

    uint64 GetAllPieces() { return m_boardState.allPieces; }

    uint64 GetBlackPieces() { return m_boardState.blackPieces; }
    uint64 GetWhitePieces() { return m_boardState.whitePieces; }

    // Function to be used for printing legal moves for a square... Shouldn't use in the engine.
    uint64 GetLegalMoves(uint64 pos);

    std::string GetStringFromMove(const Move& move);

    void SetBoardFromFEN(std::string fen);

    template<bool isWhite>
    int32 ScoreBoard();

    // Assumes the checkmask has been set already.
    bool InCheck() { return m_boardState.numPiecesChecking != 0; }

    template<bool isWhite, bool printReason = false>
    bool IsMoveLegal(const Move& move);

    uint64 GetZobKey() { return m_boardState.zobristKey; }

    void InvalidateCheckPinAndIllegalMoves() { m_boardState.illegalKingMovesValid = false;
                                               m_boardState.checkAndPinMasksValid = false;}

    uint64 GetLastPosCaptured() { return m_boardState.lastPosCaptured; }

    bool GetBoardStateIsWhiteTurn() { return m_boardState.isWhiteTurn; }

    template<bool isWhite>
    bool IsDrawByRepetition();
private:

    void ResetPieceScore();

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

    template<bool IsWhite>
    void UpdateCastleFlags(const Move& move);

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

    inline  bool IsBlack(uint64 piece) { return (m_boardState.blackPieces & piece) != 0ull; }
    inline  bool IsWhite(uint64 piece) { return (m_boardState.whitePieces & piece) != 0ull; }

    // templated for isWhite, you should know if the piece you're looking for is black or white...
    template<bool isWhite> Piece GetPieceFromPos(uint64 pos);

    template<Directions dir>
    uint64 CastRayToBlocker(uint64 pos, uint64 mask);

    template<Directions dir, bool isWhite>
    void GetCheckmaskAndPinsInDirection(uint64 pos);

    void GenerateRayTable();

    template<Piece pieceType, bool isWhite, bool hasEnPassant, bool onlyCaptures>
    void GeneratePieceMoves(Move** ppMoveList, uint32* pNumCapture, uint32* pNumNormal, uint32* pNumProbGood);

    template<bool isWhite>
    uint64 GetPawnKnightKingSeenSquares();

    template<bool isWhite>
    uint64 GetSliderSeenSquares(uint64 curKingMoves);

    void InitZobArray();

    void ResetZobKey();

    template<bool isWhite, bool hasEnPassant> uint64 GetPawnMoves      (uint64 pos);
    template<bool isWhite, bool ignoreLegal>  uint64 GetKnightMoves    (uint64 pos);
    template<bool isWhite, bool ignoreLegal>  uint64 GetRookMoves      (uint64 pos);
    template<bool isWhite, bool ignoreLegal>  uint64 GetBishopMoves    (uint64 pos);
    template<bool isWhite, bool ignoreLegal>  uint64 GetQueenMoves     (uint64 pos);
    template<bool isWhite, bool ignoreLegal>  uint64 GetKingMoves      (uint64 pos);

    template<Piece pieceType, bool isWhite, bool hasEnPassant> uint64 GetPieceMoves(uint64 pos);

    template<bool isWhite>
    void UpdateLastIrreversableMove(const Move& move);

    template<bool isWhite>
    uint64 GetPassedPawnMask()
    {
        uint64 teamPawns = GetPawn<isWhite>();
        uint64 enemyPawn = GetPawn<isWhite>();
    }

    int32 GetKingSafteyScore(uint64 whiteSeenSquares, uint64 blackSeenSquares);
    int32 GetPawnBonusScores();
    int32 GetRookBonusScores();
    int32 GetKnightBonusScores();

    uint64 m_pieces[static_cast<uint32>(Piece::PieceCount)];
    uint64 m_pRayTable[Directions::Count][64];

    std::vector<uint64> m_prevZobKeyVec;

    // the reason it isn't 64 is bc there needs to be extra spaces for ep, castling, and 
    uint64** m_ppZobristArray;

    BoardInfo m_boardState;

    bool m_fancyPrint;
};