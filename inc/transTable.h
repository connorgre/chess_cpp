#pragma once

#include "../inc/board.h"

enum class TTScoreType : uint8
{
    Exact = 0,
    LowerBound = 1, // beta cutoff
    UpperBound = 2, // didn't get over alpha
};

constexpr uint32 TTScoreTypeSize = sizeof(TTScoreType);

// Takes TransTableEntry from 48 bytes to 16 (should fit in 1 cache line).
struct TinyMove
{
    union
    {
        struct
        {
            uint32 fromIdx   : 6; // 6  -- Max val of 63  (0x3F)
            uint32 toIdx     : 6; // 12 -- Max val of 63  (0x3F)
            uint32 fromPiece : 4; // 16 -- Max val of 12  (0x0C)
            uint32 toPiece   : 4; // 20 -- Max val of 12  (0x0C)
            uint32 flags     : 9; // 29 -- Max val of 256 (0x100)
            uint32 _unused   : 3; // 32
        };

        uint32 u32all;
    };
};

struct TransTableEntry
{
    uint64      zobKey;     // 8
    TinyMove    tinyMove;   // 4
    int16       score;      // 2
    TTScoreType type;       // 1
    int8        depth;      // 1
};

class TranspositionTable
{
public:
    TranspositionTable();
    ~TranspositionTable();

    void Init(uint32 tableSize);
    void Destroy();

    Move ProbeTable(uint64 zobKey, int32 depth, int32 alpha, int32 beta);

    void InsertToTable(uint64 zobKey, int32 depth, const Move& move, TTScoreType type);

    void PrefetchEntry(uint64 zobKey);

    void ResetTable();
private:

    uint32 HashZobKey(uint64 zobKey);

    void ResetTableEntry(int32 idx);

    Move TinyMoveToMove(const TinyMove& tinyMove);
    TinyMove MoveToTinyMove(const Move& move);

    TransTableEntry* m_pTable;
    uint32 m_tableSize;
    uint32 m_hashVal;
};