#pragma once

#include "Direction.hpp"
#include "details/StableVector.hpp"
#include <SFML/Graphics/RenderWindow.hpp>
#include <vector>

inline sf::Vector2i snapToAxis(const sf::Vector2i& vec) {
    if (abs(vec.x) > abs(vec.y)) {
        return {vec.x, 0};
    } else {
        return {0, vec.y};
    }
}

inline float mag(const sf::Vector2f& vec) { return std::sqrt(vec.x * vec.x + vec.y * vec.y); }
inline float mag(const sf::Vector2i& vec) {
    return static_cast<float>(std::sqrt(vec.x * vec.x + vec.y * vec.y));
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
        return (ref == other.ref) && (portNum == other.portNum);
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
    absl::flat_hash_map<PortRef, PortRef> map2{};

  public:
    absl::flat_hash_map<PortRef, PortRef>
         map{}; // TODO maybe should be private and provide connection iterators
    bool hasInput = false;

    void insert(const Connection& con) {
        map.insert({con.portRef1, con.portRef2});
        map2.insert({con.portRef2, con.portRef1});
    }

    void erase(const Connection& con) {
        map.erase(con.portRef1);
        map.erase(con.portRef2);
        map2.erase(con.portRef1);
        map2.erase(con.portRef2);
    }

    [[nodiscard]] bool contains(const PortRef& port) const {
        return map.contains(port) || map2.contains(port);
    }

    [[nodiscard]] bool contains(const Connection& con) const {
        return (map.contains(con.portRef1) && map.find(con.portRef1)->second == con.portRef2) ||
               (map.contains(con.portRef2) && map.find(con.portRef2)->second == con.portRef1);
    }

    // prefer call contains() on port
    [[nodiscard]] bool contains(const Ref<Node> node) const {
        return contains(PortRef{node, 0}) || contains(PortRef{node, 1}) ||
               contains(PortRef{node, 2}) || contains(PortRef{node, 3});
    }

    [[nodiscard]] std::optional<Connection> getCon(const PortRef& port) const {
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

    void insert(const Connection& con, bool isNewClosNet) {
        if (isNewClosNet) {
            std::cout << "made new closed network" << std::endl;
            auto netRef = nets.insert(ClosedNet{}); // make new closedNet
            nets[netRef].insert(con);
            return;
        }

        if (typeOf(con.portRef1) == VariantType::Node && // conecting two nodes
            typeOf(con.portRef2) == VariantType::Node) {
            std::optional<Ref<ClosedNet>> net1 =
                getClosNetRef(std::get<Ref<Node>>(con.portRef1.ref));
            std::optional<Ref<ClosedNet>> net2 =
                getClosNetRef(std::get<Ref<Node>>(con.portRef2.ref));
            if (net1 && net2) {
                if (net1.value() == net2.value()) {
                    std::cout << "made loop" << std::endl;
                } else {
                    throw std::logic_error("attempted to connect two nets");
                }
            }
            // extending net with new free node
            std::cout << "extending net with new free node" << std::endl;
            nets[net1 ? net1.value() : net2.value()].insert(con);
            return;
        }

        // connecting exisitng node and obj
        const PortRef& nodePort =
            typeOf(con.portRef1) == VariantType::Node ? con.portRef1 : con.portRef2;
        nets[getClosNetRef(nodePort).value()].insert(con);
    }

    void splitCon(const Connection& con, const Ref<Node> node);

    // try to call this with ports over nodes if you have the port
    template <typename T>
    [[nodiscard]] std::optional<Ref<ClosedNet>> getClosNetRef(const T& obj) const {
        for (const auto& net: nets) {
            if (net.obj.contains(obj)) return net.ind;
        }
        return {};
    }

    [[nodiscard]] std::size_t getNodeConCount(const Ref<Node>& node) const {
        const auto& net   = nets[getClosNetRef(node).value()]; // connection assumed because node
        std::size_t count = 0;
        for (std::size_t port = 0; port < 4; ++port) {
            if (net.contains(PortRef{node, port})) ++count;
        }
        return count;
    }

    bool areConnected(const PortRef& port1, const PortRef& port2); // dijkstra's
    void remove(); // super complex becuase of potential splitting... requires isConnected
    // esentially check for
};