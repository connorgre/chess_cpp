#pragma once

#include "engine.h"
#include <vector>
#include <map>

template<typename T>
bool IsFlagSet(T value, T flag)
{
    return (value & flag) != 0ull;
}

// Sets up an engine with default values and all searches turned on.
static void SetupInitialSearchSettings(SearchSettings* pSettings)
{
    pSettings->onPv                   = true;
    pSettings->nullWindowSearch       = true;
    pSettings->useKillerMoves         = true;
    pSettings->searchReCaptureFirst   = true;

    pSettings->nullMovePrune          = true;
    pSettings->nullMoveDepth          = 4;

    pSettings->aspirationWindow       = true;
    pSettings->aspirationWindowSize   = PieceScores::PawnScore;

    pSettings->futilityPrune          = true;
    pSettings->futilityCutoff         = PieceScores::KnightScore;

    pSettings->extendedFutilityPrune  = true;
    pSettings->extendedFutilityCutoff = PieceScores::RookScore;

    pSettings->multiCutPrune          = true;
    pSettings->multiCutMoves          = 6;
    pSettings->multiCutThreshold      = 3;
    pSettings->multiCutDepth          = 3;

    pSettings->lateMoveReduction      = true;
    pSettings->numLateMovesSub        = 5;
    pSettings->numLateMovesDiv        = 10;
    pSettings->lateMoveSub            = 1;
    pSettings->lateMoveDiv            = 2;
}

enum EngineFlags : uint64
{
    NoLateMovePrune          = 1 << 0,
    NoMultiCut               = 1 << 1,
    NoKiller                 = 1 << 2,
    NoNullMove               = 1 << 3,
    NoRecaptureFirst         = 1 << 4,
    NoNullWindow             = 1 << 5,
    NoFutilityPrune          = 1 << 6,
    NoExtendedFutilityPrune  = 1 << 7,

    WeakLateMovePrune          = 1 << 8,
    WeakMultiCut               = 1 << 9,
    WeakNullMove               = 1 << 11,
    WeakFutilityPrune          = 1 << 14,
    WeakExtendedFutilityPrune  = 1 << 15,

    StrongLateMovePrune          = 1 << 16,
    StrongMultiCut               = 1 << 17,
    StrongNullMove               = 1 << 19,
    StrongFutilityPrune          = 1 << 22,
    StrongExtendedFutilityPrune  = 1 << 23,

    Default         =   0,
    NoPrune         =   NoLateMovePrune         |
                        NoMultiCut              |
                        NoNullMove              |
                        NoFutilityPrune         |
                        NoExtendedFutilityPrune,

    WeakPrune       =   WeakLateMovePrune         |
                        WeakMultiCut              |
                        WeakNullMove              |
                        WeakFutilityPrune         |
                        WeakExtendedFutilityPrune,

    StrongPrune     =   StrongLateMovePrune         |
                        StrongMultiCut              |
                        StrongNullMove              |
                        StrongFutilityPrune         |
                        StrongExtendedFutilityPrune,

    NoEnhancements   =  NoPrune          |
                        NoKiller         |
                        NoRecaptureFirst |
                        NoNullWindow,


    ErrorFlag = 0xFFFFFFFFFFFFFFFF,
};

static EngineFlags GetFlagFromString(std::string flagStr)
{
    std::map<std::string, EngineFlags> flagMap;
    flagMap["nolatemoveprune"]             = EngineFlags::NoLateMovePrune;
    flagMap["nomulticut"]                  = EngineFlags::NoMultiCut;
    flagMap["nokiller"]                    = EngineFlags::NoKiller;
    flagMap["nonullmove"]                  = EngineFlags::NoNullMove;
    flagMap["norecapturefirst"]            = EngineFlags::NoRecaptureFirst;
    flagMap["nonullwindow"]                = EngineFlags::NoNullWindow;
    flagMap["nofutilityprune"]             = EngineFlags::NoFutilityPrune;
    flagMap["noextendedfutilityprune"]     = EngineFlags::NoExtendedFutilityPrune;
    flagMap["weaklatemoveprune"]           = EngineFlags::WeakLateMovePrune;
    flagMap["weakmulticut"]                = EngineFlags::WeakMultiCut;
    flagMap["weaknullmove"]                = EngineFlags::WeakNullMove;
    flagMap["weakfutilityprune"]           = EngineFlags::WeakFutilityPrune;
    flagMap["weakextendedfutilityprune"]   = EngineFlags::WeakExtendedFutilityPrune;
    flagMap["stronglatemoveprune"]         = EngineFlags::StrongLateMovePrune;
    flagMap["strongmulticut"]              = EngineFlags::StrongMultiCut;
    flagMap["strongnullmove"]              = EngineFlags::StrongNullMove;
    flagMap["strongfutilityprune"]         = EngineFlags::StrongFutilityPrune;
    flagMap["strongextendedfutilityprune"] = EngineFlags::StrongExtendedFutilityPrune;
    flagMap["default"]                     = EngineFlags::Default;
    flagMap["noprune"]                     = EngineFlags::NoPrune;
    flagMap["weakprune"]                   = EngineFlags::WeakPrune;
    flagMap["strongprune"]                 = EngineFlags::StrongPrune;
    flagMap["noenhancements"]              = EngineFlags::NoEnhancements;

    // Check if the value exists in the map
    EngineFlags flag = EngineFlags::ErrorFlag;

    auto mapEntry = flagMap.find(flagStr);
    if (mapEntry != flagMap.end())
    {
        // Value exists, return the corresponding enum value
        flag = mapEntry->second;
    }

    return flag;
}


static SearchSettings GetSearchSetting(EngineFlags flags)
{
    SearchSettings settings = {};
    SetupInitialSearchSettings(&settings);

    if (IsFlagSet(flags, NoLateMovePrune))         { settings.lateMoveReduction     = false; }
    if (IsFlagSet(flags, NoMultiCut))              { settings.multiCutPrune         = false; }
    if (IsFlagSet(flags, NoKiller))                { settings.useKillerMoves        = false; }
    if (IsFlagSet(flags, NoNullMove))              { settings.nullMovePrune         = false; }
    if (IsFlagSet(flags, NoRecaptureFirst))        { settings.searchReCaptureFirst  = false; }
    if (IsFlagSet(flags, NoNullWindow))            { settings.nullWindowSearch      = false; }
    if (IsFlagSet(flags, NoFutilityPrune))         { settings.futilityPrune         = false; }
    if (IsFlagSet(flags, NoExtendedFutilityPrune)) { settings.extendedFutilityPrune = false; }

    if (IsFlagSet(flags, WeakLateMovePrune))
    {
        settings.numLateMovesSub = 6;
        settings.numLateMovesDiv = 12;
        settings.lateMoveSub     = 1;
        settings.lateMoveDiv     = 2;
    }

    if (IsFlagSet(flags, WeakMultiCut))
    {
        settings.multiCutDepth     = 2;
        settings.multiCutMoves     = 5;
        settings.multiCutThreshold = 3;
    }

    if (IsFlagSet(flags, WeakNullMove))
    {
        settings.nullMoveDepth = 2;
    }

    if (IsFlagSet(flags, WeakFutilityPrune))
    {
        settings.futilityCutoff = PieceScores::KnightScore + PieceScores::PawnScore;
    }

    if (IsFlagSet(flags, WeakExtendedFutilityPrune))
    {
        settings.extendedFutilityCutoff = PieceScores::RookScore + PieceScores::PawnScore;
    }

    if (IsFlagSet(flags, StrongLateMovePrune))
    {
        settings.numLateMovesSub = 4;
        settings.numLateMovesDiv = 8;
        settings.lateMoveSub     = 1;
        settings.lateMoveDiv     = 2;
    }

    if (IsFlagSet(flags, StrongMultiCut))
    {
        settings.multiCutDepth     = 4;
        settings.multiCutMoves     = 7;
        settings.multiCutThreshold = 3;
    }

    if (IsFlagSet(flags, StrongNullMove))
    {
        settings.nullMoveDepth = 5;
    }

    if (IsFlagSet(flags, StrongFutilityPrune))
    {
        settings.futilityCutoff = PieceScores::KnightScore - PieceScores::PawnScore;
    }

    if (IsFlagSet(flags, StrongExtendedFutilityPrune))
    {
        settings.extendedFutilityCutoff = PieceScores::RookScore - PieceScores::PawnScore;
    }

    return settings;
}

