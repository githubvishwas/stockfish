/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2018 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <cassert>

#include "bitboard.h"
#include "pawns.h"
#include "position.h"
#include "thread.h"

namespace {

  #define V Value
  #define S(mg, eg) make_score(mg, eg)

  // Isolated pawn penalty
  constexpr Score Isolated[VARIANT_NB] = {
    S(13, 18),
#ifdef ANTI
    S(54, 69),
#endif
#ifdef ATOMIC
    S(24, 14),
#endif
#ifdef CRAZYHOUSE
    S(30, 27),
#endif
#ifdef EXTINCTION
    S(13, 18),
#endif
#ifdef GRID
    S(13, 18),
#endif
#ifdef HORDE
    S(16, 38),
#endif
#ifdef KOTH
    S(30, 27),
#endif
#ifdef LOSERS
    S(53, 69),
#endif
#ifdef RACE
    S(0, 0),
#endif
#ifdef THREECHECK
    S(30, 27),
#endif
#ifdef TWOKINGS
    S(13, 18),
#endif
  };

  // Backward pawn penalty
  constexpr Score Backward[VARIANT_NB] = {
    S(24, 12),
#ifdef ANTI
    S(26, 50),
#endif
#ifdef ATOMIC
    S(35, 15),
#endif
#ifdef CRAZYHOUSE
    S(41, 19),
#endif
#ifdef EXTINCTION
    S(24, 12),
#endif
#ifdef GRID
    S(24, 12),
#endif
#ifdef HORDE
    S(78, 14),
#endif
#ifdef KOTH
    S(41, 19),
#endif
#ifdef LOSERS
    S(26, 49),
#endif
#ifdef RACE
    S(0, 0),
#endif
#ifdef THREECHECK
    S(41, 19),
#endif
#ifdef TWOKINGS
    S(24, 12),
#endif
  };

  // Connected pawn bonus by opposed, phalanx, #support and rank
  Score Connected[VARIANT_NB][2][2][3][RANK_NB];

  // Doubled pawn penalty
  constexpr Score Doubled[VARIANT_NB] = {
    S(18, 38),
#ifdef ANTI
    S( 4, 51),
#endif
#ifdef ATOMIC
    S( 0,  0),
#endif
#ifdef CRAZYHOUSE
    S(18, 38),
#endif
#ifdef EXTINCTION
    S(18, 38),
#endif
#ifdef GRID
    S(18, 38),
#endif
#ifdef HORDE
    S(11, 83),
#endif
#ifdef KOTH
    S(18, 38),
#endif
#ifdef LOSERS
    S( 4, 54),
#endif
#ifdef RACE
    S( 0,  0),
#endif
#ifdef THREECHECK
    S(18, 38),
#endif
#ifdef TWOKINGS
    S(18, 38),
#endif
  };

  // Weakness of our pawn shelter in front of the king by [isKingFile][distance from edge][rank].
  // RANK_1 = 0 is used for files where we have no pawns or our pawn is behind our king.
  constexpr Value ShelterWeakness[VARIANT_NB][2][int(FILE_NB) / 2][RANK_NB] = {
  {
    { { V( 98), V(20), V(11), V(42), V( 83), V( 84), V(101) }, // Not On King file
      { V(103), V( 8), V(33), V(86), V( 87), V(105), V(113) },
      { V(100), V( 2), V(65), V(95), V( 59), V( 89), V(115) },
      { V( 72), V( 6), V(52), V(74), V( 83), V( 84), V(112) } },
    { { V(105), V(19), V( 3), V(27), V( 85), V( 93), V( 84) }, // On King file
      { V(121), V( 7), V(33), V(95), V(112), V( 86), V( 72) },
      { V(121), V(26), V(65), V(90), V( 65), V( 76), V(117) },
      { V( 79), V( 0), V(45), V(65), V( 94), V( 92), V(105) } }
  },
#ifdef ANTI
  {},
#endif
#ifdef ATOMIC
  {
    { { V( 88), V(34), V( 5), V(44), V( 89), V( 90), V( 94) }, // Not On King file
      { V(116), V(61), V(-4), V(80), V( 95), V(101), V(104) },
      { V( 97), V(68), V(34), V(82), V( 62), V(104), V(110) },
      { V(103), V(44), V(44), V(77), V(103), V( 66), V(118) } },
    { { V( 88), V(34), V( 5), V(44), V( 89), V( 90), V( 94) }, // On King file
      { V(116), V(61), V(-4), V(80), V( 95), V(101), V(104) },
      { V( 97), V(68), V(34), V(82), V( 62), V(104), V(110) },
      { V(103), V(44), V(44), V(77), V(103), V( 66), V(118) } }
  },
#endif
#ifdef CRAZYHOUSE
  {
    { { V(148), V(  7), V( 84), V(141), V(156), V(177), V(326) }, // Not On King file
      { V(288), V( -3), V(141), V(216), V(182), V(213), V(162) },
      { V(190), V( 48), V(140), V(167), V(254), V(186), V(247) },
      { V(142), V(129), V( 90), V(164), V(141), V(116), V(289) } },
    { { V(145), V(-56), V( 20), V(134), V(126), V(166), V(309) }, // On King file
      { V(290), V(  0), V(144), V(222), V(177), V(210), V(161) },
      { V(205), V( 46), V(118), V(163), V(235), V(165), V(244) },
      { V(154), V( 84), V( 87), V(188), V(105), V(177), V(275) } }
  },
#endif
#ifdef EXTINCTION
  {},
#endif
#ifdef GRID
  {
    { { V( 97), V(17), V( 9), V(44), V( 84), V( 87), V( 99) }, // Not On King file
      { V(106), V( 6), V(33), V(86), V( 87), V(104), V(112) },
      { V(101), V( 2), V(65), V(98), V( 58), V( 89), V(115) },
      { V( 73), V( 7), V(54), V(73), V( 84), V( 83), V(111) } },
    { { V(104), V(20), V( 6), V(27), V( 86), V( 93), V( 82) }, // On King file
      { V(123), V( 9), V(34), V(96), V(112), V( 88), V( 75) },
      { V(120), V(25), V(65), V(91), V( 66), V( 78), V(117) },
      { V( 81), V( 2), V(47), V(63), V( 94), V( 93), V(104) } }
  },
#endif
#ifdef HORDE
  {},
#endif
#ifdef KOTH
  {
    { { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) }, // Not On King file
      { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
      { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
      { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) } },
    { { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) }, // On King file
      { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
      { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
      { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) } },
  },
#endif
#ifdef LOSERS
  {
    { { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) }, // Not On King file
      { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
      { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
      { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) } },
    { { V(100), V(20), V(10), V(46), V(82), V( 86), V( 98) }, // On King file
      { V(116), V( 4), V(28), V(87), V(94), V(108), V(104) },
      { V(109), V( 1), V(59), V(87), V(62), V( 91), V(116) },
      { V( 75), V(12), V(43), V(59), V(90), V( 84), V(112) } }
  },
#endif
#ifdef RACE
  {},
#endif
#ifdef THREECHECK
  {
    { { V(140), V( 11), V(38), V( 26), V( 99), V( 94), V( 95) }, // Not On King file
      { V(104), V( 14), V(28), V(128), V( 86), V(107), V(115) },
      { V(144), V( 59), V(89), V( 97), V( 39), V( 85), V(114) },
      { V(103), V( 24), V(76), V( 96), V(115), V( 98), V(127) } },
    { { V(115), V(-16), V(13), V( 38), V(115), V( 76), V( 92) }, // On King file
      { V(166), V( 20), V(51), V(111), V( 98), V(113), V(114) },
      { V(102), V( 29), V(76), V( 75), V( 60), V( 99), V( 96) },
      { V( 89), V( 18), V(44), V(112), V( 77), V(114), V(115) } }
  },
#endif
#ifdef TWOKINGS
  {
    { { V( 97), V(17), V( 9), V(44), V( 84), V( 87), V( 99) }, // Not On King file
      { V(106), V( 6), V(33), V(86), V( 87), V(104), V(112) },
      { V(101), V( 2), V(65), V(98), V( 58), V( 89), V(115) },
      { V( 73), V( 7), V(54), V(73), V( 84), V( 83), V(111) } },
    { { V(104), V(20), V( 6), V(27), V( 86), V( 93), V( 82) }, // On King file
      { V(123), V( 9), V(34), V(96), V(112), V( 88), V( 75) },
      { V(120), V(25), V(65), V(91), V( 66), V( 78), V(117) },
      { V( 81), V( 2), V(47), V(63), V( 94), V( 93), V(104) } }
  },
#endif
  };

  // Danger of enemy pawns moving toward our king by [type][distance from edge][rank].
  // For the unopposed and unblocked cases, RANK_1 = 0 is used when opponent has
  // no pawn on the given file, or their pawn is behind our king.
  constexpr Value StormDanger[VARIANT_NB][4][4][RANK_NB] = {
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  // BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  // Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  19), V(23), V( 1) },  // BlockedByPawn
      { V( 0),  V(   0), V(  88), V(27), V( 2) },
      { V( 0),  V(   0), V( 101), V(16), V( 1) },
      { V( 0),  V(   0), V( 111), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  // Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#ifdef ANTI
  {},
#endif
#ifdef ATOMIC
  {
    { { V(-25),  V(-332), V(-235), V( 79), V( 41) },  // BlockedByKing
      { V(-17),  V(  35), V( 206), V(-21), V(-11) },
      { V(-31),  V(  52), V( 103), V( 42), V( 94) },
      { V( -5),  V( 101), V(  67), V( 29), V( 64) } },
    { { V(-47),  V(  62), V( 114), V( 16), V( 13) },  // Unopposed
      { V( 82),  V(  41), V( 161), V( 48), V( 35) },
      { V( 44),  V(  56), V( 115), V( 17), V( 48) },
      { V(189),  V( 112), V( 202), V( 69), V(186) } },
    { { V(  1),  V( -56), V(  70), V( -5), V(-42) },  // BlockedByPawn
      { V( -2),  V( -12), V( 145), V( 56), V( 24) },
      { V(-39),  V(  32), V(  98), V( 60), V( -1) },
      { V(-11),  V( -70), V( 194), V( 58), V(138) } },
    { { V( 27),  V(  -3), V(  91), V(105), V( 27) },  // Unblocked
      { V(128),  V( -27), V(  81), V( 59), V( 27) },
      { V(126),  V(  69), V(  69), V( 33), V(  1) },
      { V(115),  V(  -7), V( 204), V( 74), V( 70) } }
  },
#endif
#ifdef CRAZYHOUSE
  {
    { { V( -54),  V(-364), V(-273), V( -2), V( 72) },  // BlockedByKing
      { V( -35),  V(  99), V( 123), V( 85), V(-25) },
      { V(   4),  V(  51), V( 136), V(111), V(149) },
      { V( -55),  V(  26), V( 164), V( 74), V( 67) } },
    { { V( 106),  V(  88), V( 213), V( 68), V( 47) },  // Unopposed
      { V( -82),  V( 122), V(  92), V(148), V(  6) },
      { V(  25),  V(   3), V( 120), V(141), V( 22) },
      { V(-109),  V(   2), V( 111), V( 26), V(-24) } },
    { { V(  34),  V(  55), V( 323), V(-12), V(-70) },  // BlockedByPawn
      { V( -55),  V( -30), V( 227), V( 19), V( 15) },
      { V(  46),  V(  -9), V( 335), V( 83), V( 66) },
      { V(-100),  V(  -4), V(  82), V( 75), V(  4) } },
    { { V(  44),  V(  37), V( 129), V( 41), V( 56) },  // Unblocked
      { V(  28),  V(  21), V( -11), V( 41), V(-71) },
      { V(   9),  V( 102), V(  77), V( 33), V( 56) },
      { V(  -2),  V(  61), V(  51), V( 56), V( -4) } }
  },
#endif
#ifdef EXTINCTION
  {},
#endif
#ifdef GRID
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  // BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  // Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  // BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  // Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#endif
#ifdef HORDE
  {
    { { V(-11),  V(-364), V(-337), V( 43), V( 69) },  // BlockedByKing
      { V(-24),  V(   2), V( 133), V(-33), V(-73) },
      { V(  9),  V(  72), V( 152), V( 99), V( 66) },
      { V( 71),  V(  18), V(  38), V( 30), V( 69) } },
    { { V( 18),  V( -11), V( 131), V( 42), V(114) },  // Unopposed
      { V( -4),  V(  63), V( -77), V( 62), V( 28) },
      { V( 66),  V(  82), V(  43), V( 11), V( 95) },
      { V(-12),  V(  45), V(  93), V(110), V( 78) } },
    { { V( 23),  V(   8), V(  86), V(-30), V(-15) },  // BlockedByPawn
      { V(105),  V(  35), V(  49), V( 78), V(-29) },
      { V(-74),  V( -27), V( 216), V( 25), V( 33) },
      { V(-14),  V(  24), V( 212), V( 80), V( -6) } },
    { { V(115),  V(  48), V( 103), V(-30), V( -9) },  // Unblocked
      { V( 67),  V(  66), V( 157), V( 38), V( 39) },
      { V( 87),  V(  48), V(  27), V(-21), V(-90) },
      { V( -7),  V(  24), V( 101), V( 90), V( 34) } }
  },
#endif
#ifdef KOTH
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  // BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  // Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  // BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  // Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#endif
#ifdef LOSERS
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  // BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  // Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  // BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  // Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#endif
#ifdef RACE
  {},
#endif
#ifdef THREECHECK
  {
    { { V(-40),  V(-310), V(-236), V( 86), V(107) },  // BlockedByKing
      { V( 24),  V(  80), V( 168), V( 38), V( -4) },
      { V( 16),  V( -41), V( 171), V( 63), V( 19) },
      { V( 12),  V(  80), V( 182), V( 36), V(-16) } },
    { { V( 27),  V( -18), V( 175), V( 31), V( 29) },  // Unopposed
      { V(106),  V(  81), V( 106), V( 86), V( 19) },
      { V( 42),  V(  62), V(  96), V( 84), V( 40) },
      { V(129),  V(  73), V( 124), V(103), V( 80) } },
    { { V(-15),  V(   9), V( -73), V(-15), V(-41) },  // BlockedByPawn
      { V(-28),  V(  28), V(  66), V( 25), V( -2) },
      { V(-38),  V( -30), V( 147), V( 24), V( 29) },
      { V(-30),  V(  39), V( 188), V(114), V( 63) } },
    { { V( 56),  V(  89), V(  34), V( -6), V(-54) },  // Unblocked
      { V( 80),  V( 123), V( 189), V( 83), V(-32) },
      { V( 89),  V(  26), V( 128), V(112), V( 78) },
      { V(166),  V(  29), V( 202), V( 18), V(109) } }
  },
#endif
#ifdef TWOKINGS
  {
    { { V( 0),  V(-290), V(-274), V(57), V(41) },  // BlockedByKing
      { V( 0),  V(  60), V( 144), V(39), V(13) },
      { V( 0),  V(  65), V( 141), V(41), V(34) },
      { V( 0),  V(  53), V( 127), V(56), V(14) } },
    { { V( 4),  V(  73), V( 132), V(46), V(31) },  // Unopposed
      { V( 1),  V(  64), V( 143), V(26), V(13) },
      { V( 1),  V(  47), V( 110), V(44), V(24) },
      { V( 0),  V(  72), V( 127), V(50), V(31) } },
    { { V( 0),  V(   0), V(  79), V(23), V( 1) },  // BlockedByPawn
      { V( 0),  V(   0), V( 148), V(27), V( 2) },
      { V( 0),  V(   0), V( 161), V(16), V( 1) },
      { V( 0),  V(   0), V( 171), V(22), V(15) } },
    { { V(22),  V(  45), V( 104), V(62), V( 6) },  // Unblocked
      { V(31),  V(  30), V(  99), V(39), V(19) },
      { V(23),  V(  29), V(  96), V(41), V(15) },
      { V(21),  V(  23), V( 116), V(41), V(15) } }
  },
#endif
  };

  // Max bonus for king safety. Corresponds to start position with all the pawns
  // in front of the king and no enemy pawn on the horizon.
  constexpr Value MaxSafetyBonus = V(258);

#ifdef HORDE
  const Score ImbalancedHorde = S(49, 39);
#endif

  #undef S
  #undef V

  template<Color Us>
  Score evaluate(const Position& pos, Pawns::Entry* e) {

    constexpr Color     Them = (Us == WHITE ? BLACK : WHITE);
    constexpr Direction Up   = (Us == WHITE ? NORTH : SOUTH);

    Bitboard b, neighbours, stoppers, doubled, supported, phalanx;
    Bitboard lever, leverPush;
    Square s;
    bool opposed, backward;
    Score score = SCORE_ZERO;
    const Square* pl = pos.squares<PAWN>(Us);

    Bitboard ourPawns   = pos.pieces(  Us, PAWN);
    Bitboard theirPawns = pos.pieces(Them, PAWN);

    e->passedPawns[Us] = e->pawnAttacksSpan[Us] = e->weakUnopposed[Us] = 0;
    e->semiopenFiles[Us] = 0xFF;
    e->kingSquares[Us]   = SQ_NONE;
    e->pawnAttacks[Us]   = pawn_attacks_bb<Us>(ourPawns);
    e->pawnsOnSquares[Us][BLACK] = popcount(ourPawns & DarkSquares);
#ifdef CRAZYHOUSE
    if (pos.is_house())
        e->pawnsOnSquares[Us][WHITE] = popcount(ourPawns & ~DarkSquares);
    else
#endif
    e->pawnsOnSquares[Us][WHITE] = pos.count<PAWN>(Us) - e->pawnsOnSquares[Us][BLACK];

#ifdef HORDE
    if (pos.is_horde() && pos.is_horde_color(Us))
    {
        int l = 0, m = 0, r = popcount(ourPawns & FileBB[FILE_A]);
        for (File f1 = FILE_A; f1 <= FILE_H; ++f1)
        {
            l = m; m = r; r = f1 < FILE_H ? popcount(ourPawns & FileBB[f1 + 1]) : 0;
            score -= ImbalancedHorde * m / (1 + l * r);
        }
    }
#endif

    // Loop through all pawns of the current color and score each pawn
    while ((s = *pl++) != SQ_NONE)
    {
        assert(pos.piece_on(s) == make_piece(Us, PAWN));

        File f = file_of(s);

        e->semiopenFiles[Us]   &= ~(1 << f);
        e->pawnAttacksSpan[Us] |= pawn_attack_span(Us, s);

        // Flag the pawn
        opposed    = theirPawns & forward_file_bb(Us, s);
        stoppers   = theirPawns & passed_pawn_mask(Us, s);
        lever      = theirPawns & PawnAttacks[Us][s];
        leverPush  = theirPawns & PawnAttacks[Us][s + Up];
#ifdef HORDE
        if (pos.is_horde() && relative_rank(Us, s) == RANK_1)
            doubled = 0;
        else
#endif
        doubled    = ourPawns   & (s - Up);
        neighbours = ourPawns   & adjacent_files_bb(f);
        phalanx    = neighbours & rank_bb(s);
#ifdef HORDE
        if (pos.is_horde() && relative_rank(Us, s) == RANK_1)
            supported = 0;
        else
#endif
        supported  = neighbours & rank_bb(s - Up);

        // A pawn is backward when it is behind all pawns of the same color on the
        // adjacent files and cannot be safely advanced.
        if (!neighbours || lever || relative_rank(Us, s) >= RANK_5)
            backward = false;
        else
        {
            // Find the backmost rank with neighbours or stoppers
            b = rank_bb(backmost_sq(Us, neighbours | stoppers));

            // The pawn is backward when it cannot safely progress to that rank:
            // either there is a stopper in the way on this rank, or there is a
            // stopper on adjacent file which controls the way to that rank.
            backward = (b | shift<Up>(b & adjacent_files_bb(f))) & stoppers;

            assert(!(backward && (forward_ranks_bb(Them, s + Up) & neighbours)));
        }

        // Passed pawns will be properly scored in evaluation because we need
        // full attack info to evaluate them. Include also not passed pawns
        // which could become passed after one or two pawn pushes when are
        // not attacked more times than defended.
        if (   !(stoppers ^ lever ^ leverPush)
            && !(ourPawns & forward_file_bb(Us, s))
            && popcount(supported) >= popcount(lever) - 1
            && popcount(phalanx)   >= popcount(leverPush))
            e->passedPawns[Us] |= s;

        else if (   stoppers == SquareBB[s + Up]
                 && relative_rank(Us, s) >= RANK_5)
        {
            b = shift<Up>(supported) & ~theirPawns;
            while (b)
                if (!more_than_one(theirPawns & PawnAttacks[Us][pop_lsb(&b)]))
                    e->passedPawns[Us] |= s;
        }

        // Score this pawn
#ifdef HORDE
        if (pos.is_horde() && relative_rank(Us, s) == RANK_1) {} else
#endif
        if (supported | phalanx)
            score += Connected[pos.variant()][opposed][bool(phalanx)][popcount(supported)][relative_rank(Us, s)];

        else if (!neighbours)
            score -= Isolated[pos.variant()], e->weakUnopposed[Us] += !opposed;

        else if (backward)
            score -= Backward[pos.variant()], e->weakUnopposed[Us] += !opposed;

#ifdef HORDE
        if (doubled && (!supported || pos.is_horde()))
#else
        if (doubled && !supported)
#endif
            score -= Doubled[pos.variant()];
    }

    return score;
  }

} // namespace

namespace Pawns {

/// Pawns::init() initializes some tables needed by evaluation. Instead of using
/// hard-coded tables, when makes sense, we prefer to calculate them with a formula
/// to reduce independent parameters and to allow easier tuning and better insight.

void init() {

  static constexpr int Seed[VARIANT_NB][RANK_NB] = {
    { 0, 13, 24, 18, 76, 100, 175, 330 },
#ifdef ANTI
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef ATOMIC
    { 0,18, 11, 14, 82,109, 170, 315 },
#endif
#ifdef CRAZYHOUSE
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef EXTINCTION
    { 0, 13, 24, 18, 76, 100, 175, 330 },
#endif
#ifdef GRID
    { 0, 13, 24, 18, 76, 100, 175, 330 },
#endif
#ifdef HORDE
    { 37, 29, 3, 1, 105,  99, 343, 350 },
#endif
#ifdef KOTH
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef LOSERS
    { 0, 8, 20, 11, 69, 91, 183, 310 },
#endif
#ifdef RACE
    {},
#endif
#ifdef THREECHECK
    { 0, 8, 19, 13, 71, 94, 169, 324 },
#endif
#ifdef TWOKINGS
    { 0, 13, 24, 18, 76, 100, 175, 330 },
#endif
  };

  for (Variant var = CHESS_VARIANT; var < VARIANT_NB; ++var)
  for (int opposed = 0; opposed <= 1; ++opposed)
      for (int phalanx = 0; phalanx <= 1; ++phalanx)
          for (int support = 0; support <= 2; ++support)
              for (Rank r = RANK_2; r < RANK_8; ++r)
  {
      int v = 17 * support;
      v += (Seed[var][r] + (phalanx ? (Seed[var][r + 1] - Seed[var][r]) / 2 : 0)) >> opposed;

#ifdef HORDE
      if (var == HORDE_VARIANT)
          Connected[var][opposed][phalanx][support][r] = make_score(v, v);
      else
#endif
      Connected[var][opposed][phalanx][support][r] = make_score(v, v * (r - 2) / 4);
  }
}


/// Pawns::probe() looks up the current position's pawns configuration in
/// the pawns hash table. It returns a pointer to the Entry if the position
/// is found. Otherwise a new Entry is computed and stored there, so we don't
/// have to recompute all when the same pawns configuration occurs again.

Entry* probe(const Position& pos) {

  Key key = pos.pawn_key();
  Entry* e = pos.this_thread()->pawnsTable[key];

  if (e->key == key)
      return e;

  e->key = key;
  e->scores[WHITE] = evaluate<WHITE>(pos, e);
  e->scores[BLACK] = evaluate<BLACK>(pos, e);
  e->openFiles = popcount(e->semiopenFiles[WHITE] & e->semiopenFiles[BLACK]);
  e->asymmetry = popcount(  (e->passedPawns[WHITE]   | e->passedPawns[BLACK])
                          | (e->semiopenFiles[WHITE] ^ e->semiopenFiles[BLACK]));

  return e;
}


/// Entry::shelter_storm() calculates shelter and storm penalties for the file
/// the king is on, as well as the two closest files.

template<Color Us>
Value Entry::shelter_storm(const Position& pos, Square ksq) {

  constexpr Color     Them = (Us == WHITE ? BLACK : WHITE);
  constexpr Direction Down = (Us == WHITE ? SOUTH : NORTH);

  enum { BlockedByKing, Unopposed, BlockedByPawn, Unblocked };

  Bitboard b = pos.pieces(PAWN) & (forward_ranks_bb(Us, ksq) | rank_bb(ksq));
  Bitboard ourPawns = b & pos.pieces(Us);
  Bitboard theirPawns = b & pos.pieces(Them);
  Value safety = MaxSafetyBonus;

  File center = std::max(FILE_B, std::min(FILE_G, file_of(ksq)));
  for (File f = File(center - 1); f <= File(center + 1); ++f)
  {
      b = ourPawns & file_bb(f);
      Rank rkUs = b ? relative_rank(Us, backmost_sq(Us, b)) : RANK_1;

      b = theirPawns & file_bb(f);
      Rank rkThem = b ? relative_rank(Us, frontmost_sq(Them, b)) : RANK_1;

      int d = std::min(f, ~f);
      safety -=  ShelterWeakness[pos.variant()][f == file_of(ksq)][d][rkUs]
               + StormDanger[pos.variant()]
                 [(shift<Down>(b) & ksq) ? BlockedByKing :
                  rkUs   == RANK_1       ? Unopposed     :
                  rkThem == (rkUs + 1)   ? BlockedByPawn : Unblocked]
                 [d][rkThem];
  }

  return safety;
}


/// Entry::do_king_safety() calculates a bonus for king safety. It is called only
/// when king square changes, which is about 20% of total king_safety() calls.

template<Color Us>
Score Entry::do_king_safety(const Position& pos, Square ksq) {

  kingSquares[Us] = ksq;
  castlingRights[Us] = pos.can_castle(Us);
  int minKingPawnDistance = 0;

  Bitboard pawns = pos.pieces(Us, PAWN);
  if (pawns)
      while (!(DistanceRingBB[ksq][minKingPawnDistance++] & pawns)) {}

  Value bonus = shelter_storm<Us>(pos, ksq);

  // If we can castle use the bonus after the castling if it is bigger
  if (pos.can_castle(MakeCastling<Us, KING_SIDE>::right))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_G1)));

  if (pos.can_castle(MakeCastling<Us, QUEEN_SIDE>::right))
      bonus = std::max(bonus, shelter_storm<Us>(pos, relative_square(Us, SQ_C1)));

#ifdef CRAZYHOUSE
  if (pos.is_house())
      return make_score(bonus, bonus);
#endif
  return make_score(bonus, -16 * minKingPawnDistance);
}

// Explicit template instantiation
template Score Entry::do_king_safety<WHITE>(const Position& pos, Square ksq);
template Score Entry::do_king_safety<BLACK>(const Position& pos, Square ksq);

} // namespace Pawns