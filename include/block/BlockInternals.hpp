#pragma once

#include "details/StableVector.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <cstddef>
#include <variant>
#include <vector>

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

inline bool isVecInDir(const sf::Vector2i& vec, Direction dir) {
    assert(isVecHoriVert(vec));
    return dot(vec, dirToVec(dir)) > 0; // if dot prouct > 0 then must be in same dir
}

inline Direction vecToDir(const sf::Vector2i& vec){
    assert(isVecHoriVert(vec));
    if (vec.y != 0) {
        return vec.y < 0 ? Direction::up : Direction::down;
    }
    return vec.x < 0 ? Direction::left : Direction::right;
}

inline int magPolar(const sf::Vector2i& vec) { return abs(vec.x) + abs(vec.y); };

inline bool isVecBetween(const sf::Vector2i& vec, const sf::Vector2i& end1,
                         const sf::Vector2i& end2) {
    assert(isVecHoriVert(end1 - end2));
    // imagine end1 is origin
    return magPolar(vec - end1) + magPolar(end2 - vec) == magPolar(end2 - end1);
}

inline sf::Vector2i snapToAxis(const sf::Vector2i& vec) {
    if (abs(vec.x) > abs(vec.y)) {
        return {vec.x, 0};
    } else {
        return {0, vec.y};
    }
}

class PortInst {
  public:
    Direction    portDir;
    sf::Vector2i portPos;
    bool         isOutput;
};

class Node {
  public:
    sf::Vector2i            pos;
    std::array<PortInst, 4> ports;

    // Always form a connection to node after construction
    Node(const sf::Vector2i& pos_) : pos(pos_) {
        ports[0] = {Direction::up, pos, false};
        ports[1] = {Direction::down, pos, false};
        ports[2] = {Direction::left, pos, false};
        ports[3] = {Direction::right, pos, false};
    }
};

class BlockInst {
  public:
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
};

class Gate {
  public:
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
};

using PortObjRef = std::variant<Ref<Node>, Ref<Gate>, Ref<BlockInst>>;
enum struct VariantType : std::size_t { Node = 0, Gate = 1, BlockInst = 2 };

class PortRef {
  public:
    PortObjRef  ref;
    std::size_t portNum;
    PortRef(PortObjRef ref_, std::size_t portNum_) : ref(ref_), portNum(portNum_) {}

    bool operator==(const PortRef& other) const {
        return (ref == other.ref) || (portNum == other.portNum);
    }

    bool isConnected(const PortRef& other) const {
        if (VariantType(other.ref.index()) ==
            VariantType::Node) { // ports are only connected within a node (for now)
            return ref == other.ref;
        }
        return false;
    }
};

inline VariantType typeOf(const PortObjRef& ref) { return VariantType(ref.index()); }
inline VariantType typeOf(const PortRef& ref) { return typeOf(ref.ref); }

template <>
struct std::hash<PortRef> {
    std::size_t operator()(const PortRef& portRef) const {
        return std::hash<PortObjRef>{}(portRef.ref) + std::hash<std::size_t>{}(portRef.portNum);
    }
};

class Connection {
  public:
    PortRef portRef1;
    PortRef portRef2;

    bool operator==(const Connection& other) const { // compare is commutative
        return (portRef1 == other.portRef1 && portRef2 == other.portRef2) ||
               (portRef1 == other.portRef2 && portRef1 == other.portRef2);
    }

    Connection getSwapped() const { return {portRef2, portRef1}; }
};

template <>
struct std::hash<Connection> { // hash function commutative
    std::size_t operator()(const Connection& con) const {
        return std::hash<PortRef>{}(con.portRef1) + std::hash<PortRef>{}(con.portRef2);
    }
};

class ClosedNet {
  private:
    std::unordered_map<PortRef, PortRef> map2{};

  public:
    std::unordered_map<PortRef, PortRef>
         map{}; // TODO should be private and provide connection iterators
    bool hasInput = false;

    void insert(const Connection& con) {
        map.insert({con.portRef1, con.portRef2});
        map2.insert({con.portRef2, con.portRef1});
    }

    void erase(const Connection& con) {
        map.erase(con.portRef1);
        map.erase(con.portRef2);
        map2.erase(con.portRef1);
        map2.erase(con.portRef1);
    }

    bool contains(const PortRef& port) const { return map.contains(port) || map2.contains(port); }

    bool contains(const Ref<Node> node) const {
        return contains(PortRef{node, 0}) || contains(PortRef{node, 1}) ||
               contains(PortRef{node, 2}) || contains(PortRef{node, 3});
    }

    std::optional<Connection> getCon(const PortRef& port) const {
        if (map.contains(port)) {
            return Connection{port, map.find(port)->second};
        } else if (map2.contains(port)) {
            return Connection{port, map2.find(port)->second};
        }
        return {};
    }

    void operator+=(const ClosedNet& other) { // add all connections from another network
        for (const auto& con: other.map) {
            insert({con.first, con.second});
        }
    }
};

class ConnectionNetwork {
  public:
    StableVector<ClosedNet> nets{};

    void insert(const Connection& con) {
        if (nets.size() == 0){
            auto net = nets.insert(ClosedNet{});
            nets[net].insert(con);
        }
        if (typeOf(con.portRef1) == VariantType::Node &&
            typeOf(con.portRef2) == VariantType::Node) { // conecting two nodes
            Ref<ClosedNet> net1 = nets.front().ind;      // default values becuase references
            Ref<ClosedNet> net2 = nets.front().ind;      // default values becuase references
            for (const auto& net: nets) {
                if (net.obj.contains(con.portRef1)) {
                    net1 = net.ind;
                }
                if (net.obj.contains(con.portRef2)) {
                    net2 = net.ind;
                }
            }
            if (&net1 == &net2) { // connecting within closed net
                nets[net1].insert(con);
            } else { // connecting two closed nets
                // TODO
            }
        } else if (typeOf(con.portRef1) == VariantType::Node || // connecting node and obj
                   typeOf(con.portRef2) == VariantType::Node) {
            // TODO
        } else { // connecting two objects
                 // TODO
        }
    }

    void splitCon(const Connection& con, const Ref<Node> node);

    std::optional<Ref<ClosedNet>> getNet(const PortRef& port) const {
        for (const auto& net: nets) {
            if (net.obj.contains(port)) return net.ind;
        }
        return {};
    }

    Ref<ClosedNet> getNet(const Ref<Node>& node) const {
        for (std::size_t port = 0; port < 4; ++port) {
            auto netRef = getNet(PortRef{node, port});
            if (netRef) return netRef.value();
        }
        throw std::logic_error("node has no connections. Should never happen");
    }

    bool isConnected(const PortRef& port1, const PortRef& port2); // dijkstra's
    void remove(); // super complex becuase of potential splitting... requires isConnected
    // esentially check for
};