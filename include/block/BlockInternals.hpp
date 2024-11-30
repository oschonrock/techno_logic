#pragma once

#include "Direction.hpp"
#include "details/StableVector.hpp"

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

struct Block;

// TODO maybe remove... really only used when inserting for entering in out/inputs
// Is annoying elsewhere
enum struct PortType { input, output, node };

class PortInst {
  public:
    Direction    portDir;
    sf::Vector2i portPos;
    bool         negated = false;
};

class Node {
  public:
    sf::Vector2i            pos;
    std::array<PortInst, 4> ports;

    // Always form a connection to node after construction
    Node(const sf::Vector2i& pos_) : pos(pos_) {
        ports[0] = {Direction::up, pos};
        ports[1] = {Direction::down, pos};
        ports[2] = {Direction::left, pos};
        ports[3] = {Direction::right, pos};
    }
};

class BlockInst {
  public:
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
    Ref<Block>            block;
};

class Gate {
  public:
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
};

using PortObjRef = std::variant<Ref<Node>, Ref<Gate>, Ref<BlockInst>>;
enum struct PortObjType : std::size_t { Node = 0, Gate = 1, BlockInst = 2 };
static constexpr std::array<std::string, 3> PortObjRefStrings{"node", "gate", "block"};

class PortRef {
  public:
    PortObjRef  ref;
    std::size_t portNum;
    PortType    portType;
    PortRef(PortObjRef ref_, std::size_t portNum_, PortType portType_)
        : ref(ref_), portNum(portNum_), portType(portType_) {}

    bool operator==(const PortRef& other) const {
        return (ref == other.ref) && (portNum == other.portNum);
    }
};

inline PortObjType typeOf(const PortObjRef& ref) { return PortObjType(ref.index()); }
inline PortObjType typeOf(const PortRef& ref) { return typeOf(ref.ref); }

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
    absl::flat_hash_map<PortRef, PortRef> conMap{};
    absl::flat_hash_map<PortRef, PortRef> conMap2{};
    std::optional<PortRef>                input{};   // only 1 allowed atm
    std::vector<PortRef>                  outputs{}; // TODO maybe unnecessary data duplication
    std::size_t                           size{};

    void maintainIOVecs(bool isInsert, const PortRef& portRef) {
        if (portRef.portType == PortType::node) return;
        if (portRef.portType == PortType::input) {
            if (isInsert) {
                if (input) throw std::logic_error("Tried to add two inputs to closed graph");
            } else {
                input.reset();
            }
        } else {
            if (isInsert) {
                outputs.push_back(portRef);
            } else {
                auto it = std::find(outputs.begin(), outputs.end(), portRef);
                if (it != outputs.end())
                    throw std::logic_error("Tried to remove output that isn't in closed graph");
                std::erase(outputs, *it);
            }
        }
    }

  public:
    [[nodiscard]] const absl::flat_hash_map<PortRef, PortRef>& getMap() const { return conMap; }
    [[nodiscard]] bool                          hasInput() const { return input.has_value(); }
    [[nodiscard]] const std::optional<PortRef>& getInput() const { return input; }
    [[nodiscard]] const std::vector<PortRef>&   getOutputs() const { return outputs; }
    [[nodiscard]] std::size_t                   getSize() const { return size; }

    void insert(const Connection& con) {
        conMap.insert({con.portRef1, con.portRef2});
        conMap2.insert({con.portRef2, con.portRef1});
        ++size;
        maintainIOVecs(true, con.portRef1);
        maintainIOVecs(true, con.portRef2);
    }

    void erase(const Connection& con) {
        conMap.erase(con.portRef1);
        conMap.erase(con.portRef2);
        conMap2.erase(con.portRef1);
        conMap2.erase(con.portRef2);
        --size;
        maintainIOVecs(false, con.portRef1);
        maintainIOVecs(false, con.portRef2);
    }

    [[nodiscard]] bool contains(const PortRef& port) const {
        return conMap.contains(port) || conMap2.contains(port);
    }

    [[nodiscard]] bool contains(const Connection& con) const {
        return (conMap.contains(con.portRef1) &&
                conMap.find(con.portRef1)->second == con.portRef2) ||
               (conMap.contains(con.portRef2) && conMap.find(con.portRef2)->second == con.portRef1);
    }

    // prefer call contains() on port
    [[nodiscard]] bool contains(const Ref<Node> node) const {
        return contains(PortRef{node, 0, PortType::node}) ||
               contains(PortRef{node, 1, PortType::node}) ||
               contains(PortRef{node, 2, PortType::node}) ||
               contains(PortRef{node, 3, PortType::node});
    }

    [[nodiscard]] Connection getCon(const PortRef& port) const {
        if (conMap.contains(port)) {
            return Connection{port, conMap.find(port)->second};
        } else if (conMap2.contains(port)) {
            return Connection{port, conMap2.find(port)->second};
        }
        throw std::logic_error("Port not connected to closed net. Did you call contains?");
    }

    // NOTE: Destroy network "other". Adds all connections from another network
    void operator+=(const ClosedNet& other) {
        for (const auto& con: other.conMap) {
            insert({con.first, con.second});
        }
    }
};

class ConnectionNetwork {
  public:
    StableVector<ClosedNet> nets{};

    void insert(const Connection& con, const std::optional<Ref<ClosedNet>>& net1,
                const std::optional<Ref<ClosedNet>>& net2) {
        if (!net1 && !net2) { // make new closedNet
            std::cout << "made new closed network \n";
            auto netRef = nets.insert(ClosedNet{});
            nets[netRef].insert(con);
            return;
        }

        if (net1 && net2) { // connecting two existing networks
            auto net1Ref = net1.value();
            auto net2Ref = net2.value();
            if (net1Ref == net2Ref) {
                std::cout << "made loop within closed network \n";
                nets[net1Ref].insert(con);
                return;
            } else {
                std::cout << "connected two closed networks \n";
                if (nets[net1Ref].getSize() < nets[net2Ref].getSize()) std::swap(net1Ref, net2Ref);
                // net1 is now the bigger of the two
                nets[net1Ref] += nets[net2Ref];
                nets.erase(net2Ref);
                nets[net1Ref].insert(con);
                return;
            }
        }
        std::cout << "extending network \n";
        nets[net1 ? net1.value() : net2.value()].insert(con);
        return;
    }

    // try to call this with ports over nodes if you have the port
    template <typename T>
    [[nodiscard]] std::optional<Ref<ClosedNet>> getClosNetRef(const T& obj) const {
        for (const auto& net: nets) {
            if (net.obj.contains(obj)) return net.ind;
        }
        return {};
    }

    template <typename T>
    [[nodiscard]] bool contains(const T& obj) const {
        return getClosNetRef(obj).has_value();
    }

    [[nodiscard]] std::size_t getNodeConCount(const Ref<Node>& node) const {
        const auto& net   = nets[getClosNetRef(node).value()]; // connection assumed because node
        std::size_t count = 0;
        for (std::size_t port = 0; port < 4; ++port) {
            if (net.contains(PortRef{node, port, PortType::node})) ++count;
        }
        return count;
    }

    bool areConnected(const PortRef& port1, const PortRef& port2); // dijkstra's
    void remove(); // super complex becuase of potential splitting... requires isConnected
    // esentially check for
};