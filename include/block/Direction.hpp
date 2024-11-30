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

inline Direction reverseDirection(Direction dir) {
    int val = static_cast<int>(dir);
    val += val % 2 == 0 ? 1 : -1;
    return Direction(val);
}

// note 0,0 = false
inline bool isVecHoriVert(const sf::Vector2i& vec) { return ((vec.x != 0) != (vec.y != 0)); }

inline int dot(const sf::Vector2i& a, const sf::Vector2i& b) { return a.x * b.x + a.y * b.y; }

// inline bool isVecInDir(const sf::Vector2i& vec, Direction dir) {
//     assert(isVecHoriVert(vec));
//     return dot(vec, dirToVec(dir)) > 0; // if dot prouct > 0 then must be in same dir
// }

inline Direction vecToDir(const sf::Vector2i& vec) {
    assert(isVecHoriVert(vec));
    if (vec.y != 0) {
        return vec.y < 0 ? Direction::up : Direction::down;
    }
    return vec.x < 0 ? Direction::left : Direction::right;
}

inline int magPolar(const sf::Vector2i& vec) { return abs(vec.x) + abs(vec.y); }

inline bool isVecBetween(const sf::Vector2i& vec, const sf::Vector2i& end1,
                         const sf::Vector2i& end2) {
    assert(isVecHoriVert(end1 - end2));
    // imagine end1 is origin
    return magPolar(vec - end1) + magPolar(end2 - vec) == magPolar(end2 - end1);
}