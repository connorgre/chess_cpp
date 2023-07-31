#include "util.h"
#include "immintrin.h"
#include "intrin0.h"

enum U64Walls : uint64
{
    Bottom = 0x00000000000000FFull,
    Top    = 0xFF00000000000000ull,
    Left   = 0x0101010101010101ull,
    Right  = 0x8080808080808080ull,
    Border = 0xFF818181818181FFull
};

// takes 0-63 and gets the u64 for it
static constexpr uint64 IndexToPosition(uint32 idx) { return 1ull << idx; }

// Helper functions to determine if the 0-63 index is on an edge
// of the board
static constexpr bool IsIndexOnTopEdge(uint32 idx) { return idx >= A8; }
static constexpr bool IsIndexOnBottomEdge(uint32 idx) { return idx <= H1; }
static constexpr bool IsIndexOnRightEdge(uint32 idx) { return (idx % 8) == H1; }
static constexpr bool IsIndexOnLeftEdge(uint32 idx) { return (idx % 8) == A1; }

static constexpr uint64 GetLSB(uint64 pos) { return pos & (~(pos-1)); }
// Should try to avoid using this, slower than just GetLSB
static constexpr uint64 GetMSB(uint64 pos)
{ 
    uint64 bit = pos;
    uint64 lsb = GetLSB(bit);
    // constantly take the LSB off in a loop
    while (bit != lsb)
    {
        bit &= ~lsb;
        lsb = GetLSB(bit);
    }
    return bit;
}

static constexpr uint32 PopCount(uint64 pos)
{
    // supposedly, this popcount should be faster than using
    // the intrinsic...
    //return _mm_popcnt_u64(pos);
    constexpr uint64 k1 = 0x5555555555555555;
    constexpr uint64 k2 = 0x3333333333333333;
    constexpr uint64 k4 = 0x0f0f0f0f0f0f0f0f;
    constexpr uint64 kf = 0x0101010101010101;
    pos = pos - ((pos >> 1)& k1); 
    pos = (pos & k2) + ((pos >> 2)& k2);
    pos = (pos + (pos >> 4))& k4;
    pos = (pos * kf) >> 56;
    return pos;
}

// Moves piece by 1, 0 for bits that wrap around the board
static constexpr uint64 MoveUp(uint64 pos)    { return (pos & ~U64Walls::Top)    << 8; }
static constexpr uint64 MoveDown(uint64 pos)  { return (pos & ~U64Walls::Bottom) >> 8; }
static constexpr uint64 MoveLeft(uint64 pos)  { return (pos & ~U64Walls::Left)   >> 1; }
static constexpr uint64 MoveRight(uint64 pos) { return (pos & ~U64Walls::Right)  << 1; }

// Moves piece by N, returns 0 for bits that wrap around the board
static constexpr uint64 MoveLeftByN(uint64 pos, uint32 n)
{
    for (uint32 idx = 0; idx < n; idx++)
    {
        pos = (pos >> 1) & (~U64Walls::Right);
    }
    return pos;
}
static constexpr uint64 MoveRightByN(uint64 pos, uint32 n)
{
    for (uint32 idx = 0; idx < n; idx++)
    {
        pos = (pos << 1) & (~U64Walls::Left);
    }
    return pos;
}

static uint64 GenerateRayInDirection(uint64 pos, Directions direction)
{
    uint64 ray     = pos;
    uint64 prevRay = 0ull;
    while (ray != prevRay)
    {
        prevRay = ray;
        if      (direction == Directions::North) ray |= MoveUp(ray);
        else if (direction == Directions::East)  ray |= MoveRight(ray);
        else if (direction == Directions::South) ray |= MoveDown(ray);
        else if (direction == Directions::West)  ray |= MoveLeft(ray);

        else if (direction == Directions::NorthEast)  ray |= MoveRight(MoveUp(ray));
        else if (direction == Directions::NorthWest)  ray |= MoveLeft(MoveUp(ray));
        else if (direction == Directions::SouthEast)  ray |= MoveRight(MoveDown(ray));
        else if (direction == Directions::SouthWest)  ray |= MoveLeft(MoveUp(ray));
        
    }

    return ray;
}
static constexpr uint64 MoveUpLeft(uint64 pos)    
    { return (pos & ~(U64Walls::Top    | U64Walls::Left))  << 7; }
static constexpr uint64 MoveUpRight(uint64 pos)    
    { return (pos & ~(U64Walls::Top    | U64Walls::Right)) << 9; }
static constexpr uint64 MoveDownLeft(uint64 pos)    
    { return (pos & ~(U64Walls::Bottom | U64Walls::Left))  >> 9; }
static constexpr uint64 MoveDownRight(uint64 pos)    
    { return (pos & ~(U64Walls::Bottom | U64Walls::Right)) >> 7; }

static void GetIndividualBits(uint64 pieces, uint64* pieceList)
{
    uint64 pos = GetLSB(pieces);
    uint32 idx = 0;
    while (pos != 0)
    {
        pieceList[idx++] = pos;
        pieces &= ~pos;
        pos = GetLSB(pieces);
    }
    pieceList[idx] = 0ull;
}

static uint32 GetIndex(uint64 pos)
{ 
    uint32 index = 0;
    _BitScanForward64(&index, pos);
    return index;
}

static uint32 GetRank(uint64 pos) { return GetIndex(pos) % 8; }
static uint32 GetFile(uint64 pos) { return 7 - (GetIndex(pos) / 8); }
