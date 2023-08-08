#include "../inc/board.h"
#include "../inc/bitHelper.h"
// This file is the implementation of the move generation functions for the Board class.  It is a
// Separate function because there is quite a bit that goes into it.

// rayToHit is the ray to the enemy that put us in check
// rayToPin is the ray through our piece to a piec
template<Directions dir, bool isWhite>
void Board::GetCheckmaskAndPinsInDirection(uint64 pos)
{
    // is horizontal/vertical or diagonal direction
    constexpr bool isHV = ((dir == North) || (dir == East) || (dir == South) || (dir == West));

    const uint64 enemySlideMask = GetQueen<!isWhite>() | ((isHV) ? GetRook<!isWhite>() :
                                                                   GetBishop<!isWhite>());

    uint32 posIdx = GetIndex(pos);
    uint64 ray = m_rayTable[dir][posIdx];

    // endOfRay will be a 1 bit mask of the enemy sliding piece that sees the king (piece in pos)
    uint64 endOfRay = ray & enemySlideMask;
    // These need LSB to get the first blocker of the array
    if constexpr ((dir == North)     || (dir == East) ||
                  (dir == NorthEast) || (dir == NorthWest))
    {
        endOfRay = GetLSB(endOfRay);
    }
    else
    {
        endOfRay = GetMSB(endOfRay);
    }

    const uint64 endOfRayIdx = GetIndex(endOfRay);
    // If there was no piece putting the king under attack, cutoffRay should just be set to mask
    // out the whole ray.
    const bool rayIsZero = endOfRay == 0ull;
    const uint64 cutoffRay = (rayIsZero) ? ray : m_rayTable[dir][endOfRayIdx];

    const uint64 rayToEnemySlider = ray & ~cutoffRay;

    // numPiecesInRay will be:
    //  0  -- no enemy slider looking in the direction of our king, nothing to do
    //  1  -- enemy slider checking our king
    //  2  -- enemy slider pinning a piece to our king
    //  3  -- 2 pieces pinned together.  Only relevant for checking en passant legality
    //  >3 -- Nothing to do.
    const uint32 numPiecesInRay = PopCount(rayToEnemySlider & m_boardState.allPieces);

    // Now set the appropriate masks
    m_boardState.checkMask |= (numPiecesInRay == 1) ? rayToEnemySlider : 0ull;
    m_boardState.numPiecesChecking += (numPiecesInRay == 1);

    // This will help prune king moves by eliminating checks that go through the king.  This should
    // only be a single instruction because dir is templated.
    uint64 squareBehindRay = (dir == North) ? MoveDown(pos)  :
                             (dir == East)  ? MoveLeft(pos)  :
                             (dir == South) ? MoveUp(pos)    :
                             (dir == West)  ? MoveRight(pos) :
                             (dir == NorthEast) ? MoveDownLeft(pos)  :
                             (dir == NorthWest) ? MoveDownRight(pos) :
                             (dir == SouthEast) ? MoveUpLeft(pos)    :
                                                  MoveUpRight(pos);
    m_boardState.illegalKingMoveMask |= (numPiecesInRay == 1) ? squareBehindRay : 0ull;

    if constexpr (isHV)
    {
        m_boardState.hvPinMask       |= (numPiecesInRay == 2) ? rayToEnemySlider : 0ull;
    }
    else
    {
        m_boardState.diagPinMask |= (numPiecesInRay == 2) ? rayToEnemySlider : 0ull;
    }
    if constexpr ((dir == East) || (dir == West))
    {
        m_boardState.doubleHorizontalPinMask |= (numPiecesInRay == 3) ? rayToEnemySlider : 0ull;
    }
}

template void Board::GetCheckmaskAndPinsInDirection<North,     true>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<East,      true>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<South,     true>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<West,      true>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<NorthEast, true>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<NorthWest, true>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<SouthEast, true>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<SouthWest, true>(uint64 pos);

template void Board::GetCheckmaskAndPinsInDirection<North,     false>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<East,      false>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<South,     false>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<West,      false>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<NorthEast, false>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<NorthWest, false>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<SouthEast, false>(uint64 pos);
template void Board::GetCheckmaskAndPinsInDirection<SouthWest, false>(uint64 pos);

template<Directions dir>
uint64 Board::CastRayToBlocker(uint64 pos, uint64 mask)
{
    constexpr bool needLsb = ((dir == North) || (dir == East) || 
                              (dir == NorthEast) || (dir == NorthWest));

    uint32 posIdx = GetIndex(pos);
    uint64 ray = m_rayTable[dir][posIdx];

    uint64 endOfRay = ray & mask;
    if constexpr (needLsb)
    {
        endOfRay = GetLSB(endOfRay);
    }
    else
    {
        endOfRay = GetMSB(endOfRay);
    }

    const uint64 endOfRayIdx = GetIndex(endOfRay);

    // If there was no piece blocking us, return just the ray
    const bool noCutoff = endOfRay == 0ull;
    return (noCutoff) ? ray : ray ^ m_rayTable[dir][endOfRayIdx];
}

// This will generate a mask of all moves to get us out of check, and all pins to the king
template<bool isWhite>
void Board::GenerateCheckAndPinMask()
{
    const uint64 kingPos = GetKing<isWhite>();
    m_boardState.checkMask               = 0ull;
    m_boardState.hvPinMask               = 0ull;
    m_boardState.diagPinMask             = 0ull;
    m_boardState.doubleHorizontalPinMask = 0ull;
    m_boardState.illegalKingMoveMask     = 0ull;
    m_boardState.numPiecesChecking       = 0ull;
    m_boardState.legalCastles            = 0ull;

    // If we are in check by a knight
    m_boardState.checkMask = GetKnightMoves<isWhite, true>(kingPos) & GetKnight<!isWhite>();

    // If we are in check by a pawn.
    if constexpr (isWhite)
    {
        m_boardState.checkMask |= (MoveUpRight(kingPos) | MoveUpLeft(kingPos))     & GetPawn<!isWhite>();
    }
    else
    {
        m_boardState.checkMask |= (MoveDownRight(kingPos) | MoveDownLeft(kingPos)) & GetPawn<!isWhite>();
    }

    // We can never be checked by a pawn and knight at the same time, so increment the number of
    // pieces checking by 1 if we're checked by either.
    m_boardState.numPiecesChecking += (m_boardState.checkMask != 0ull);

    GetCheckmaskAndPinsInDirection<North,     isWhite>(kingPos);
    GetCheckmaskAndPinsInDirection<East,      isWhite>(kingPos);
    GetCheckmaskAndPinsInDirection<South,     isWhite>(kingPos);
    GetCheckmaskAndPinsInDirection<West,      isWhite>(kingPos);
    GetCheckmaskAndPinsInDirection<NorthEast, isWhite>(kingPos);
    GetCheckmaskAndPinsInDirection<NorthWest, isWhite>(kingPos);
    GetCheckmaskAndPinsInDirection<SouthEast, isWhite>(kingPos);
    GetCheckmaskAndPinsInDirection<SouthWest, isWhite>(kingPos);

    // If nobody is checking us, we can move anywhere.
    m_boardState.checkMask = (m_boardState.checkMask == 0ull) ? FullBoard : m_boardState.checkMask;
}

template void Board::GenerateCheckAndPinMask<true>();
template void Board::GenerateCheckAndPinMask<false>();

// Generates all legal moves.  I should probably relax king 'seenSquares' rules, and only check
// those on the first king move.  This should avoid the most expensive part of the check.
template<bool isWhite>
void Board::GenerateLegalMoves(Move* moveList, uint32* pNumMoves)
{
    GenerateCheckAndPinMask<isWhite>();

    // If we're in a double check, then we can only move the king.  Don't bother generating the
    // rest of the moves
    if (m_boardState.numPiecesChecking < 2)
    {
        // need to move this further out
        if (m_boardState.enPassantSquare != 0ull)
        {
            GeneratePieceMoves<wPawn, isWhite, true>(moveList, pNumMoves);
        }
        else
        {
            GeneratePieceMoves<wPawn,   isWhite, false>(moveList, pNumMoves);
        }
        GeneratePieceMoves<wKnight, isWhite, false>(moveList, pNumMoves);
        GeneratePieceMoves<wBishop, isWhite, false>(moveList, pNumMoves);
        GeneratePieceMoves<wRook,   isWhite, false>(moveList, pNumMoves);
        GeneratePieceMoves<wQueen,  isWhite, false>(moveList, pNumMoves);
    }
    GeneratePieceMoves<wKing,   isWhite, false>(moveList, pNumMoves);

}

template<Piece pieceType, bool isWhite, bool hasEnPassant>
void Board::GeneratePieceMoves(Move* pMoveList, uint32* pNumMoves)
{
    uint64 pieces        = GetPieces<pieceType, isWhite>();
    uint64 enemySquares = (isWhite) ? m_boardState.blackPieces : m_boardState.whitePieces;

    // The pieceType passed in is only actually the white pieces.
    constexpr uint32 pieceTypeOffset = (isWhite) ? wKing : bKing;
    while (pieces != 0ull)
    {
        uint64 piece = GetLSB(pieces);
        pieces ^= piece;

        uint64 moves = GetPieceMoves<pieceType, isWhite, hasEnPassant>(piece);

        uint64 attacks = moves & enemySquares;
        moves ^= attacks;

        // Handle en passant
        if constexpr (hasEnPassant)
        {
            uint64 enPassantSquare = moves & m_boardState.enPassantSquare;
            moves ^= enPassantSquare;
            if (enPassantSquare != 0ull)
            {
                pMoveList[*pNumMoves].fromPiece = (isWhite) ? wPawn : bPawn;
                pMoveList[*pNumMoves].toPiece   = (isWhite) ? bPawn : wPawn;
                pMoveList[*pNumMoves].fromPos   = piece;
                pMoveList[*pNumMoves].toPos     = enPassantSquare;
                pMoveList[*pNumMoves].flags     = MoveFlags::EnPassant;

                *pNumMoves += 1;
            }
        }

        // Handle castling
        if constexpr (pieceType == wKing)
        {
            // Since the castle move only actually cares about the flag, we only need to fill that
            // out.  We can also avoid branching by only conditionally incrementing the number of
            // moves.
            uint64 castleFlags = m_boardState.legalCastles;

            uint64 flag = GetLSB(castleFlags);
            castleFlags ^= flag;
            pMoveList[*pNumMoves].flags = flag;
            (*pNumMoves) += (flag == 0ull) ? 0 : 1;

            pMoveList[*pNumMoves].flags = castleFlags;
            (*pNumMoves) += (castleFlags == 0ull) ? 0 : 1;
        }

        while (attacks != 0ull)
        {
            uint64 attack = GetLSB(attacks);
            attacks ^= attack;

            MoveFlags flags = MoveFlags::NoFlag;

            // handle promotions
            if constexpr (pieceType == wPawn)
            {
                uint64 promotionRow = (isWhite) ? MoveDown(Top) : MoveUp(Bottom);
                bool isPromotion = piece & promotionRow;

                if (isPromotion)
                {
                    pMoveList[*pNumMoves].fromPiece = static_cast<Piece>(pieceType + pieceTypeOffset);
                    pMoveList[*pNumMoves].toPiece = GetPieceFromPos<!isWhite>(attack);
                    pMoveList[*pNumMoves].fromPos = piece;
                    pMoveList[*pNumMoves].toPos = attack;
                    pMoveList[*pNumMoves].flags = MoveFlags::QueenPromotion;
                    *pNumMoves += 1;

                    pMoveList[*pNumMoves].fromPiece = static_cast<Piece>(pieceType + pieceTypeOffset);
                    pMoveList[*pNumMoves].toPiece = GetPieceFromPos<!isWhite>(attack);
                    pMoveList[*pNumMoves].fromPos = piece;
                    pMoveList[*pNumMoves].toPos = attack;
                    pMoveList[*pNumMoves].flags = MoveFlags::KnightPromotion;
                    *pNumMoves += 1;

                    pMoveList[*pNumMoves].fromPiece = static_cast<Piece>(pieceType + pieceTypeOffset);
                    pMoveList[*pNumMoves].toPiece = GetPieceFromPos<!isWhite>(attack);
                    pMoveList[*pNumMoves].fromPos = piece;
                    pMoveList[*pNumMoves].toPos = attack;
                    pMoveList[*pNumMoves].flags = MoveFlags::RookPromotion;
                    *pNumMoves += 1;

                    // let the normal insertion handle this
                    flags = MoveFlags::BishopPromotion;
                }
            }

            pMoveList[*pNumMoves].fromPiece = static_cast<Piece>(pieceType + pieceTypeOffset);
            pMoveList[*pNumMoves].toPiece   = GetPieceFromPos<!isWhite>(attack);
            pMoveList[*pNumMoves].fromPos   = piece;
            pMoveList[*pNumMoves].toPos     = attack;
            pMoveList[*pNumMoves].flags     = flags;
            *pNumMoves += 1;
        }

        while (moves != 0ull)
        {
            uint64 move = GetLSB(moves);
            moves ^= move;

            MoveFlags flags = MoveFlags::NoFlag;

            // handle promotions
            if constexpr (pieceType == wPawn)
            {
                uint64 promotionRow = (isWhite) ? MoveDown(Top) : MoveUp(Bottom);
                bool isPromotion = piece & promotionRow;

                if (isPromotion)
                {
                    pMoveList[*pNumMoves].fromPiece = static_cast<Piece>(pieceType + pieceTypeOffset);
                    pMoveList[*pNumMoves].toPiece = Piece::NoPiece;
                    pMoveList[*pNumMoves].fromPos = piece;
                    pMoveList[*pNumMoves].toPos = move;
                    pMoveList[*pNumMoves].flags = MoveFlags::QueenPromotion;
                    *pNumMoves += 1;

                    pMoveList[*pNumMoves].fromPiece = static_cast<Piece>(pieceType + pieceTypeOffset);
                    pMoveList[*pNumMoves].toPiece = Piece::NoPiece;
                    pMoveList[*pNumMoves].fromPos = piece;
                    pMoveList[*pNumMoves].toPos = move;
                    pMoveList[*pNumMoves].flags = MoveFlags::KnightPromotion;
                    *pNumMoves += 1;

                    pMoveList[*pNumMoves].fromPiece = static_cast<Piece>(pieceType + pieceTypeOffset);
                    pMoveList[*pNumMoves].toPiece = Piece::NoPiece;
                    pMoveList[*pNumMoves].fromPos = piece;
                    pMoveList[*pNumMoves].toPos = move;
                    pMoveList[*pNumMoves].flags = MoveFlags::RookPromotion;
                    *pNumMoves += 1;

                    // let the normal insertion handle this
                    flags = MoveFlags::BishopPromotion;
                }
            }

            pMoveList[*pNumMoves].fromPiece = static_cast<Piece>(pieceType + pieceTypeOffset);
            pMoveList[*pNumMoves].toPiece   = Piece::NoPiece;
            pMoveList[*pNumMoves].fromPos   = piece;
            pMoveList[*pNumMoves].toPos     = move;
            pMoveList[*pNumMoves].flags     = flags;
            *pNumMoves += 1;
        }
    }
}

//=================================================================================================
template<Piece pieceType, bool isWhite, bool hasEnPassant> 
uint64 Board::GetPieceMoves(uint64 pos)
{
    if      constexpr (pieceType == wKing)   { return GetKingMoves<isWhite, false>(pos);   }
    else if constexpr (pieceType == wQueen)  { return GetQueenMoves<isWhite, false>(pos);  }
    else if constexpr (pieceType == wRook)   { return GetRookMoves<isWhite, false>(pos);   }
    else if constexpr (pieceType == wBishop) { return GetBishopMoves<isWhite, false>(pos); }
    else if constexpr (pieceType == wKnight) { return GetKnightMoves<isWhite, false>(pos); }
    else if constexpr (pieceType == wPawn)   { return GetPawnMoves<isWhite, hasEnPassant>(pos);   }
    else static_assert(false);
}

template<bool isWhite, bool hasEnPassant>
uint64 Board::GetPawnMoves(uint64 pos)
{
    uint64 legalEnPassantSquare = 0ull;
    if constexpr (hasEnPassant)
    {
        // en passant is illegal if the pawn we are taking is in the diagonal pin mask
        bool illegalBecauseDiagPin = false;
        if constexpr (isWhite)
        {
            illegalBecauseDiagPin = (MoveDown(m_boardState.enPassantSquare) & m_boardState.diagPinMask) != 0ull;
        }
        else
        {
            illegalBecauseDiagPin = (MoveUp(m_boardState.enPassantSquare) & m_boardState.diagPinMask) != 0ull;
        }

        // Also illegal if we are part of the double pin to the king.
        bool illegalBecauseDoublePin = (pos & m_boardState.doubleHorizontalPinMask) != 0ull;

        const bool enPassantIllegal = illegalBecauseDiagPin || illegalBecauseDoublePin;
        legalEnPassantSquare = (enPassantIllegal) ? 0ull : m_boardState.enPassantSquare;
    }

    uint64 pushes  = 0ull;
    uint64 attacks = 0ull;

    // If we are pinned in a direction, we can only move along the pinmask
    const uint64 legalHvPinMoves   = ((pos & m_boardState.hvPinMask) != 0) ? m_boardState.hvPinMask : FullBoard;
    const uint64 legalDiagPinMoves = ((pos & m_boardState.diagPinMask) != 0) ? m_boardState.diagPinMask : FullBoard;

    if constexpr (isWhite)
    {
        // Get the pawn pushes
        pushes = MoveUp(pos & ~m_boardState.diagPinMask) & legalHvPinMoves & ~m_boardState.allPieces;

        const bool canDoublePush = pos & MoveUp(Bottom);
        pushes |= (canDoublePush) ? MoveUp(pushes) : 0ull;
        pushes &= ~m_boardState.allPieces;

        // Get attacking moves
        attacks = (MoveUpLeft(pos & ~m_boardState.hvPinMask) |
                   MoveUpRight(pos & ~m_boardState.hvPinMask)) & legalDiagPinMoves
                                                    & (m_boardState.blackPieces | legalEnPassantSquare);
    }
    else
    {
        // Get the pawn pushes
        pushes = MoveDown(pos & ~m_boardState.diagPinMask) & legalHvPinMoves & ~m_boardState.allPieces;

        const bool canDoublePush = pos & MoveDown(Top);
        pushes |= (canDoublePush) ? MoveDown(pushes) : 0ull;
        pushes &= ~m_boardState.allPieces;

        // Get attacking moves
        attacks = (MoveDownLeft(pos & ~m_boardState.hvPinMask) |
                   MoveDownRight(pos & ~m_boardState.hvPinMask)) & legalDiagPinMoves
                                                      & (m_boardState.whitePieces | legalEnPassantSquare);
    }
    return (pushes | attacks) & m_boardState.checkMask;
}

template<bool isWhite, bool ignoreLegal>
uint64 Board::GetKnightMoves(uint64 pos)
{
    uint64 legalKnightMoves = 0ull;
    // if the knight is in either pin mask, it can't move
    if constexpr (ignoreLegal == false)
    {
        pos &= (~m_boardState.diagPinMask & ~m_boardState.hvPinMask);
    }
    const uint64 movedUp    = MoveUp(pos);
    const uint64 movedLeft  = MoveLeft(pos);
    const uint64 movedRight = MoveRight(pos);
    const uint64 movedDown  = MoveDown(pos);

    legalKnightMoves |= MoveUpRight(movedUp);
    legalKnightMoves |= MoveUpLeft(movedUp);

    legalKnightMoves |= MoveUpLeft(movedLeft);
    legalKnightMoves |= MoveDownLeft(movedLeft);

    legalKnightMoves |= MoveUpRight(movedRight);
    legalKnightMoves |= MoveDownRight(movedRight);

    legalKnightMoves |= MoveDownLeft(movedDown);
    legalKnightMoves |= MoveDownRight(movedDown);

    if constexpr (ignoreLegal == false)
    {
        const uint64 teamPieces = (isWhite) ? m_boardState.whitePieces : m_boardState.blackPieces;
        legalKnightMoves &= ~teamPieces;
    
        legalKnightMoves &= m_boardState.checkMask;
    }
    return legalKnightMoves;
}

template<bool isWhite, bool ignoreLegal>
uint64 Board::GetBishopMoves(uint64 pos)
{
    uint64 moves = CastRayToBlocker<NorthEast>(pos, m_boardState.allPieces) |
                   CastRayToBlocker<NorthWest>(pos, m_boardState.allPieces) |
                   CastRayToBlocker<SouthEast>(pos, m_boardState.allPieces) |
                   CastRayToBlocker<SouthWest>(pos, m_boardState.allPieces);

    if constexpr (ignoreLegal == false)
    {
        const bool isPinnedDiag = (pos & m_boardState.diagPinMask) != 0ull;
        const bool isPinnedHv   = (pos & m_boardState.hvPinMask) != 0ull;

        moves = (isPinnedDiag) ? (moves & m_boardState.diagPinMask) : moves;
        moves = (isPinnedHv) ? 0ull : moves;
        moves &= m_boardState.checkMask;

        const uint64 teamPieces = (isWhite) ? m_boardState.whitePieces : m_boardState.blackPieces;
        moves &= ~teamPieces;
    }

    return moves;
}

template<bool isWhite, bool ignoreLegal>
uint64 Board::GetRookMoves(uint64 pos)
{
    uint64 moves = CastRayToBlocker<North>(pos, m_boardState.allPieces) |
                   CastRayToBlocker<East>(pos, m_boardState.allPieces)  |
                   CastRayToBlocker<South>(pos, m_boardState.allPieces) |
                   CastRayToBlocker<West>(pos, m_boardState.allPieces);

    if constexpr (ignoreLegal == false)
    {
        const bool isPinnedDiag = (pos & m_boardState.diagPinMask) != 0ull;
        const bool isPinnedHv = (pos & m_boardState.hvPinMask) != 0ull;

        moves = (isPinnedHv) ? (moves & m_boardState.hvPinMask) : moves;
        moves = (isPinnedDiag) ? 0ull : moves;
        moves &= m_boardState.checkMask;

        const uint64 teamPieces = (isWhite) ? m_boardState.whitePieces : m_boardState.blackPieces;
        moves &= ~teamPieces;
    }
    return moves;
}

template<bool isWhite, bool ignoreLegal>
uint64 Board::GetQueenMoves(uint64 pos)
{
    return GetBishopMoves<isWhite, ignoreLegal>(pos) | GetRookMoves<isWhite, ignoreLegal>(pos);
}

// I should move the checking seen squares out of here and check them on the first attempted move
// by the king, as most king moves will probably never be explored
template<bool isWhite, bool ignoreLegal>
uint64 Board::GetKingMoves(uint64 pos)
{
    uint64 kingMoves = 0ull;
    kingMoves |= MoveUp(pos);
    kingMoves |= MoveLeft(pos);
    kingMoves |= MoveRight(pos);
    kingMoves |= MoveDown(pos);
    kingMoves |= MoveUpLeft(pos);
    kingMoves |= MoveUpRight(pos);
    kingMoves |= MoveDownLeft(pos);
    kingMoves |= MoveDownRight(pos);

    if constexpr (ignoreLegal == false)
    {
        // This doesn't fully prune all king moves, but should get rid of some clearly illegal moves.
        // Can't move into check
        uint64 illegalMoves = (m_boardState.checkMask == FullBoard) ? 0ull : m_boardState.checkMask;
        // We can move onto the checkmask only if we are capturing the checking piece
        illegalMoves &= ~((isWhite) ? m_boardState.blackPieces : m_boardState.whitePieces);
        // Can't move onto our own team
        illegalMoves |= ((isWhite) ? m_boardState.whitePieces : m_boardState.blackPieces);
        // Can't move onto a square behing a sliding piece giving check
        illegalMoves |= m_boardState.illegalKingMoveMask;

        uint64 enemySeenSquares = GetPawnKnightKingSeenSquares<!isWhite>();
        enemySeenSquares |= GetSliderSeenSquares<!isWhite>(kingMoves);

        kingMoves &= ~illegalMoves;
        kingMoves &= ~enemySeenSquares;

        const uint64 seenAndOccupiedSquares = enemySeenSquares | m_boardState.allPieces;
        if constexpr (isWhite)
        {
            const uint64 kingSideSafeSquares = MoveRight(pos) | WhiteKingSideCastleLand;
            const bool kingSideCastle = (m_boardState.checkMask == FullBoard) &&
                                        ((kingSideSafeSquares & seenAndOccupiedSquares) == 0ull) &&
                                        ((m_boardState.castleMask & WhiteKingCastle) != 0);

            const uint64 queenSideSafeSquares = MoveLeft(pos) | WhiteQueenSideCastleLand;
            const bool queenSideCastle = (m_boardState.checkMask == FullBoard) &&
                                        ((queenSideSafeSquares & seenAndOccupiedSquares) == 0ull) &&
                                        ((m_boardState.castleMask & WhiteQueenCastle) != 0) &&
                                        ((m_boardState.allPieces & MoveRight(WhiteQueenSideRookStart)) == 0);

            m_boardState.legalCastles |= (kingSideCastle)  ? WhiteKingCastle  : 0;
            m_boardState.legalCastles |= (queenSideCastle) ? WhiteQueenCastle : 0;
        }
        else
        {
            const uint64 kingSideSafeSquares = MoveRight(pos) | BlackKingSideCastleLand;
            const bool kingSideCastle = (m_boardState.checkMask == FullBoard) &&
                                        ((kingSideSafeSquares & seenAndOccupiedSquares) == 0ull) &&
                                        ((m_boardState.castleMask & BlackKingCastle) != 0);

            const uint64 queenSideSafeSquares = MoveLeft(pos) | BlackQueenSideCastleLand;
            const bool queenSideCastle = (m_boardState.checkMask == FullBoard) &&
                                         ((queenSideSafeSquares & seenAndOccupiedSquares) == 0ull) &&
                                         ((m_boardState.castleMask & BlackQueenCastle) != 0) &&
                                         ((m_boardState.allPieces & MoveRight(BlackQueenSideRookStart)) == 0);

            m_boardState.legalCastles |= (kingSideCastle)  ? BlackKingCastle  : 0;
            m_boardState.legalCastles |= (queenSideCastle) ? BlackQueenCastle : 0;
        }
        
    }

    return kingMoves;
}

// To be used for pruning king moves.  Returns all the squares that could be attacked by pawns,
// knights, and the enemy king.
template<bool isWhite>
uint64 Board::GetPawnKnightKingSeenSquares()
{
    uint64 seenSquares = 0ull;
    uint64 knights = GetKnight<isWhite>();
    uint64 pawns   = GetPawn<isWhite>();
    uint64 kingPos = GetKing<isWhite>();

    seenSquares |= GetKnightMoves<isWhite, true>(knights);
    if constexpr (isWhite)
    {
        seenSquares |= MoveUpLeft(pawns) | MoveUpRight(pawns);
    }
    else
    {
        seenSquares |= MoveDownLeft(pawns) | MoveDownRight(pawns);
    }

    seenSquares |= GetKingMoves<isWhite, true>(kingPos);

    return seenSquares;
}

template uint64 Board::GetPawnKnightKingSeenSquares<true>();
template uint64 Board::GetPawnKnightKingSeenSquares<false>();

// Gets all the squares seen by enemy sliders.  Assumes checkmask is filled out.
// Almost 1/4 of total perft time is spent in this function.
template<bool isWhite>
uint64 Board::GetSliderSeenSquares(uint64 curKingMoves)
{
    // If we already found a rook or bishop checking us, then there is no need to check it's moves
    // again.  This doesn't work for the queen though, since it can check us, and cut us off on a
    // differnt line.
    uint64 alreadyInMask = (m_boardState.checkMask == FullBoard) ? 0ull : m_boardState.checkMask;

    uint64 seenSquares = 0ull;

    // If a rook right next to our king, it might be cutting the king off.
    uint64 rooks   = GetRook<isWhite>()   & ~(alreadyInMask & ~curKingMoves);
    uint64 bishops = GetBishop<isWhite>() & ~alreadyInMask;
    uint64 queens  = GetQueen<isWhite>();

    while (rooks != 0ull)
    {
        uint64 rook = GetLSB(rooks);
        rooks ^= rook;
        seenSquares |= GetRookMoves<isWhite, true>(rook);
    }
    while (queens != 0ull)
    {
        uint64 queen = GetLSB(queens);
        queens ^= queen;
        seenSquares |= GetQueenMoves<isWhite, true>(queen);
    }
    while (bishops != 0ull)
    {
        uint64 bishop = GetLSB(bishops);
        bishops ^= bishop;
        seenSquares |= GetBishopMoves<isWhite, true>(bishop);
    }

    return seenSquares;
}

template uint64 Board::GetSliderSeenSquares<true>(uint64 curKingMoves);
template uint64 Board::GetSliderSeenSquares<false>(uint64 curKingMoves);

template<bool isWhite>
Piece Board::GetPieceFromPos(uint64 pos)
{
    Piece piece = NoPiece;
    if constexpr (isWhite)
    {
        piece = (IsWhiteKing(pos))   ? wKing   :
                (IsWhiteQueen(pos))  ? wQueen  :
                (IsWhiteRook(pos))   ? wRook   :
                (IsWhiteBishop(pos)) ? wBishop :
                (IsWhiteKnight(pos)) ? wKnight :
                (IsWhitePawn(pos))   ? wPawn   :
                NoPiece;
    }
    else
    {
        piece = (IsBlackKing(pos))   ? bKing   :
                (IsBlackQueen(pos))  ? bQueen  :
                (IsBlackRook(pos))   ? bRook   :
                (IsBlackBishop(pos)) ? bBishop :
                (IsBlackKnight(pos)) ? bKnight :
                (IsBlackPawn(pos))   ? bPawn   :
                NoPiece;
    }
    return piece;
}

template Piece Board::GetPieceFromPos<true>(uint64 pos);
template Piece Board::GetPieceFromPos<false>(uint64 pos);

uint64 Board::GetLegalMoves(uint64 pos)
{
    bool isWhite = IsWhite(pos);
    if (isWhite)
    {
        GenerateCheckAndPinMask<true>();
    }
    else
    {
        GenerateCheckAndPinMask<false>();
    }

    uint64 legalMoves = 0ull;
    if      (IsWhiteKing(pos))   legalMoves = GetKingMoves<true, false>(pos);
    else if (IsWhiteQueen(pos))  legalMoves = GetQueenMoves<true, false>(pos);
    else if (IsWhiteRook(pos))   legalMoves = GetRookMoves<true, false>(pos);
    else if (IsWhiteBishop(pos)) legalMoves = GetBishopMoves<true, false>(pos);
    else if (IsWhiteKnight(pos)) legalMoves = GetKnightMoves<true, false>(pos);
    else if (IsWhitePawn(pos))   legalMoves = GetPawnMoves<true, false>(pos);

    else if (IsBlackKing(pos))   legalMoves = GetKingMoves<false, false>(pos);
    else if (IsBlackQueen(pos))  legalMoves = GetQueenMoves<false, false>(pos);
    else if (IsBlackRook(pos))   legalMoves = GetRookMoves<false, false>(pos);
    else if (IsBlackBishop(pos)) legalMoves = GetBishopMoves<false, false>(pos);
    else if (IsBlackKnight(pos)) legalMoves = GetKnightMoves<false, false>(pos);
    else if (IsBlackPawn(pos))   legalMoves = GetPawnMoves<false, false>(pos);

    if ((m_boardState.numPiecesChecking > 1) && (IsWhiteKing(pos) == false) && (IsBlackKing(pos) == false))
    {
        legalMoves = 0ull;
    }

    return legalMoves;

}
//=================================================================================================
template void Board::GenerateLegalMoves<true>(Move* moveList, uint32* numMoves);
template void Board::GenerateLegalMoves<false>(Move* moveList, uint32* numMoves);

//=================================================================================================
template uint64 Board::GetPawnMoves<true, true>(uint64 pos);
template uint64 Board::GetPawnMoves<false, true>(uint64 pos);
template uint64 Board::GetPawnMoves<true, false>(uint64 pos);
template uint64 Board::GetPawnMoves<false, false>(uint64 pos);

template uint64 Board::GetKnightMoves<true, true>(uint64 pos);
template uint64 Board::GetKnightMoves<false, true>(uint64 pos);
template uint64 Board::GetKnightMoves<true, false>(uint64 pos);
template uint64 Board::GetKnightMoves<false, false>(uint64 pos);

template uint64 Board::GetBishopMoves<true, true>(uint64 pos);
template uint64 Board::GetBishopMoves<false, true>(uint64 pos);
template uint64 Board::GetBishopMoves<true, false>(uint64 pos);
template uint64 Board::GetBishopMoves<false, false>(uint64 pos);

template uint64 Board::GetRookMoves<true, true>(uint64 pos);
template uint64 Board::GetRookMoves<false, true>(uint64 pos);
template uint64 Board::GetRookMoves<true, false>(uint64 pos);
template uint64 Board::GetRookMoves<false, false>(uint64 pos);

template uint64 Board::GetQueenMoves<true, true>(uint64 pos);
template uint64 Board::GetQueenMoves<false, true>(uint64 pos);
template uint64 Board::GetQueenMoves<true, false>(uint64 pos);
template uint64 Board::GetQueenMoves<false, false>(uint64 pos);

template uint64 Board::GetKingMoves<true, true>(uint64 pos);
template uint64 Board::GetKingMoves<false, true>(uint64 pos);
template uint64 Board::GetKingMoves<true, false>(uint64 pos);
template uint64 Board::GetKingMoves<false, false>(uint64 pos);

template uint64 Board::GetPieceMoves<wKing,   true, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wQueen,  true, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wRook,   true, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wBishop, true, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wKnight, true, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wPawn,   true, false>(uint64 pos);

template uint64 Board::GetPieceMoves<wKing,   false, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wQueen,  false, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wRook,   false, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wBishop, false, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wKnight, false, false>(uint64 pos);
template uint64 Board::GetPieceMoves<wPawn,   false, false>(uint64 pos);

// pawns are the only pieces we need to instantiate en passant templates for
template uint64 Board::GetPieceMoves<wPawn, true, true>(uint64 pos);
template uint64 Board::GetPieceMoves<wPawn, false, true>(uint64 pos);

// ================================================================================================
template void Board::GeneratePieceMoves<wKing,   true, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wQueen,  true, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wRook,   true, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wBishop, true, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wKnight, true, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wPawn,   true, false>(Move* pMoveList, uint32* pNumMoves);

template void Board::GeneratePieceMoves<wKing,   false, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wQueen,  false, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wRook,   false, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wBishop, false, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wKnight, false, false>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wPawn,   false, false>(Move* pMoveList, uint32* pNumMoves);

// pawns are the only pieces we need to instantiate en passant templates for
template void Board::GeneratePieceMoves<wPawn, true, true>(Move* pMoveList, uint32* pNumMoves);
template void Board::GeneratePieceMoves<wPawn, false, true>(Move* pMoveList, uint32* pNumMoves);