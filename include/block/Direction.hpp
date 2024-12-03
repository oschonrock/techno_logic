#pragma once

#include <SFML/System/Vector2.hpp>
#include <cassert>
#include <cmath>
#include <cstddef>
enum struct Direction : std::size_t { up = 0, down = 1, left = 2, right = 3 };

inline sf::Vector2i dirToVec(Direction dir) {
    switch (dir) {
    case Direction::up:
        return {0, -1};
    case Direction::down:
        return {0, 1};
    case Direction::left:
        return {-1, 0};
    case Direction::right:
        return {1, 0};
    }
}

// note 0,0 = false
inline bool isVecHoriVert(const sf::Vector2i& vec) { return ((vec.x != 0) != (vec.y != 0)); }

inline int magPolar(const sf::Vector2i& vec) { return abs(vec.x) + abs(vec.y); }

inline Direction vecToDir(const sf::Vector2i& vec) {
    assert(isVecHoriVert(vec));
    if (vec.y != 0) {
        return vec.y < 0 ? Direction::up : Direction::down;
    }
    return vec.x < 0 ? Direction::left : Direction::right;
}

inline int dot(const sf::Vector2i& a, const sf::Vector2i& b) { return a.x * b.x + a.y * b.y; }

inline int dot(const Direction& a, const sf::Vector2i& b) { return dot(dirToVec(a), b); }

inline Direction reverseDir(Direction dir) {
    int val = static_cast<int>(dir);
    val += val % 2 == 0 ? 1 : -1;
    return Direction(val);
}

inline Direction swapXY(Direction dir) {
    auto vec = dirToVec(dir);
    std::swap(vec.x, vec.y);
    return vecToDir(vec);
}

// NOTE: not inclusive
inline bool isVecBetween(const sf::Vector2i& vec, const sf::Vector2i& end1,
                         const sf::Vector2i& end2) {
    assert(isVecHoriVert(end1 - end2));
    auto mag1 = magPolar(vec - end1);
    auto mag2 = magPolar(end2 - vec);
    if (mag1 == 0 || mag2 == 0) return false; // exclude ends
    return mag1 + mag2 == magPolar(end2 - end1);
}

inline bool doLinesCross(const std::pair<sf::Vector2i, sf::Vector2i>& line1,
                         const std::pair<sf::Vector2i, sf::Vector2i>& line2) {
    auto diff1 = line1.second - line1.first;
    auto diff2 = line2.second - line2.first;
    assert(isVecHoriVert(diff1));
    assert(isVecHoriVert(diff2));
    auto dir1 = vecToDir(diff1);
    auto dir2 = vecToDir(diff2);
    if (dir1 == dir2 || dir1 == reverseDir(dir2)) return false; // if parralel
    auto dt = dot(diff1, line2.first - line1.first);
    if (!(dt > 0 && dt < (magPolar(diff1) ^ 2))) return false; // line 2 not between line 1
    dt = dot(diff2, line1.first - line2.first);
    if (!(dt > 0 && dt < (magPolar(diff2) ^ 2))) return false; // line 1 not between line 2
    return true;
}

// inline bool isVecInDir(const sf::Vector2i& vec, Direction dir) {
//     assert(isVecHoriVert(vec));
//     return dot(vec, dirToVec(dir)) > 0; // if dot prouct > 0 then must be in same dir
// }