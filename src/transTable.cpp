#include "../inc/transTable.h"
#include "../inc/engine.h"
#include "../inc/bitHelper.h"
#include <intrin.h>

TranspositionTable::TranspositionTable()
:
m_pTable(nullptr),
m_hashVal(0),
m_tableSize(0)
{

}

TranspositionTable::~TranspositionTable()
{

}

void TranspositionTable::Init(uint32 tableSize)
{
    m_tableSize = tableSize;
    m_hashVal = tableSize;

    m_pTable = static_cast<TransTableEntry*>(malloc(tableSize * sizeof(TransTableEntry)));
    ResetTable();
}

void TranspositionTable::Destroy()
{
    free(m_pTable);
    m_pTable = nullptr;
}

void TranspositionTable::PrefetchEntry(uint64 zobKey)
{
    uint32 entryIdx = HashZobKey(zobKey);
    const char* pEntry = reinterpret_cast<char*>(&(m_pTable[entryIdx]));
    _mm_prefetch(pEntry, _MM_HINT_NTA);
}

Move TranspositionTable::ProbeTable(
    uint64 zobKey, 
    int32 depth, 
    int32 alpha, 
    int32 beta)
{
    uint32 entryIdx = HashZobKey(zobKey);

    TransTableEntry tableEntry = m_pTable[entryIdx];

    Move move = {};
    move.score = TTScoreNotFound;
    
    // We found a match
    if ((tableEntry.zobKey == zobKey) && (tableEntry.depth >= depth))
    {
        move = TinyMoveToMove(tableEntry.tinyMove);
        move.score = tableEntry.score;
        if ((tableEntry.type == TTScoreType::LowerBound) && (move.score <= alpha))
        {
            move.score = alpha;
        }
        else if ((tableEntry.type == TTScoreType::UpperBound) && (move.score >= beta))
        {
            move.score = beta;
        }
        else if (tableEntry.type != TTScoreType::Exact)
        {
            move.score = InvalidScore;
        }
    }
    else if (tableEntry.zobKey == zobKey)
    {
        move = TinyMoveToMove(tableEntry.tinyMove);
        move.score = InvalidScore;
    }

    return move;
}

void TranspositionTable::InsertToTable(
    uint64 zobKey,
    int32 depth,
    const Move& move, 
    TTScoreType type)
{
    uint32 entryIdx = HashZobKey(zobKey);
    TransTableEntry* pTableEntry = &(m_pTable[entryIdx]);
    if (pTableEntry->depth <= depth)
    {
        TinyMove tinyMove = MoveToTinyMove(move);
        pTableEntry->depth    = depth;
        pTableEntry->tinyMove = tinyMove;
        pTableEntry->type     = type;
        pTableEntry->zobKey   = zobKey;
        pTableEntry->score    = move.score;
    }
}

uint32 TranspositionTable::HashZobKey(uint64 zobKey)
{
    return zobKey % m_tableSize;
}

void TranspositionTable::ResetTableEntry(int32 idx)
{
    m_pTable[idx].depth              = -1;
    m_pTable[idx].zobKey             = FullBoard;
    m_pTable[idx].score              = TTScoreNotFound;
    m_pTable[idx].type               = TTScoreType::LowerBound;
    m_pTable[idx].tinyMove.fromPiece = Piece::NoPiece;
}

void TranspositionTable::ResetTable()
{
    for (uint32 idx = 0; idx < m_tableSize; idx++)
    {
        ResetTableEntry(idx);
    }
}

TinyMove TranspositionTable::MoveToTinyMove(const Move& move)
{
    TinyMove tinyMove = {};
    tinyMove.fromIdx   = GetIndex(move.fromPos);
    tinyMove.toIdx     = GetIndex(move.toPos);
    tinyMove.fromPiece = move.fromPiece;
    tinyMove.toPiece   = move.toPiece;
    tinyMove.flags     = move.flags;

    return tinyMove;
}

Move TranspositionTable::TinyMoveToMove(const TinyMove& tinyMove)
{
    Move move = {};
    move.fromPos   = IndexToPosition(tinyMove.fromIdx);
    move.toPos     = IndexToPosition(tinyMove.toIdx);
    move.fromPiece = static_cast<Piece>(tinyMove.fromPiece);
    move.toPiece   = static_cast<Piece>(tinyMove.toPiece);
    move.flags     = tinyMove.flags;

    return move;
}