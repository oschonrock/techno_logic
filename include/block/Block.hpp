#pragma once

#include "Helpers.hpp"
#include "details/StableVector.hpp"

struct PortInst {
    Direction    portDir;
    sf::Vector2i portPos;
    bool         negated = false;
};

struct Node {
    sf::Vector2i            pos;
    std::array<PortInst, 4> ports;

    // Always form a connection to node after construction
    Node(const sf::Vector2i& pos_);
};

struct Block;

struct BlockInst {
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
    Ref<Block>            block;
};

struct Gate {
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
};

using PortObjRef = std::variant<Ref<Node>, Ref<Gate>, Ref<BlockInst>>;
enum struct PortObjType : std::size_t { Node = 0, Gate = 1, BlockInst = 2 };
static constexpr std::array<std::string, 3> PortObjRefStrings{"node", "gate", "block"};

enum struct PortType { input, output, node };

struct Port {
    std::string name;
    PortType    type;
};

struct PortRef {
    PortObjRef  ref;
    std::size_t portNum;
    PortRef(PortObjRef ref_, std::size_t portNum_) : ref(ref_), portNum(portNum_) {}

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
    std::optional<PortRef>                input{}; // only 1 allowed atm
    std::vector<PortRef>                  outputs{};
    std::size_t                           size{};

    void maintainIOVecs(bool isInsert, const PortRef& portRef, const PortType& portType);

  public:
    // [[nodiscard]] const absl::flat_hash_map<PortRef, PortRef>& getMap() const { return conMap; }
    [[nodiscard]] bool                          hasInput() const { return input.has_value(); }
    [[nodiscard]] const std::optional<PortRef>& getInput() const { return input; }
    [[nodiscard]] const std::vector<PortRef>&   getOutputs() const { return outputs; }
    [[nodiscard]] std::size_t                   getSize() const { return size; }

    void insert(const Connection& con, const std::pair<PortType, PortType>& portTypes);
    void erase(const Connection& con, const std::pair<PortType, PortType>& portTypes);
    [[nodiscard]] bool contains(const PortRef& port) const;
    [[nodiscard]] bool contains(const Connection& con) const;
    // prefer call contains() on port
    [[nodiscard]] bool       contains(const Ref<Node> node) const;
    [[nodiscard]] Connection getCon(const PortRef& port) const;

    // NOTE: Destroys network "other". Adds all connections from another network
    void operator+=(const ClosedNet& other) {
        for (const auto& con: other.conMap) {
            insert({con.first, con.second}, {PortType::node, PortType::node});
        }
        conMap.begin();
    }

    // const iterator to loop over connections
    struct Iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = Connection;
        using pointer           = const Connection*; // or also value_type*
        using reference         = const Connection&; // or also value_type&

        using MapIt = absl::flat_hash_map<PortRef, PortRef>::const_iterator;
        Iterator()  = delete;
        Iterator(MapIt it, MapIt end) : it_(it), end_(end), con(it->first, it->second) {}
        Iterator(MapIt it, Connection placeholderCon) : it_(it), con(placeholderCon) {}

        [[nodiscard]] reference operator*() const { return con; }
        [[nodiscard]] pointer   operator->() const { return &con; }

        Iterator& operator++() { // Prefix increment
            ++it_;
            if (it_ != end_) con = Connection(it_->first, it_->second);
            return *this;
        }

        Iterator operator++(int) { // Postfix increment
            Iterator tmp = *this;
            ++it_;
            if (it_ != end_) con = Connection(it_->first, it_->second);
            return tmp;
        }

        friend bool operator==(const Iterator& a, const Iterator& b) { return a.it_ == b.it_; }
        friend bool operator!=(const Iterator& a, const Iterator& b) { return a.it_ != b.it_; }

      private:
        MapIt      it_;
        MapIt      end_; // required to avoid dereference of end()
        Connection con;
    };
    static_assert(std::input_iterator<Iterator>);

    [[nodiscard]] Iterator begin() const { return Iterator(conMap.begin(), conMap.end()); }
    [[nodiscard]] Iterator end() const {
        // passes placeholder connection to prevent *end() exploding
        return Iterator(conMap.end(), Connection(*begin()));
    }
};

class ConnectionNetwork {
  private:
    void copyConnectedPorts(Block& block, const ClosedNet& sourceNet, ClosedNet& destNet,
                            const PortRef& port);

  public:
    StableVector<ClosedNet> nets{};
    void                    insert(const Connection& con, const std::optional<Ref<ClosedNet>>& net1,
                                   const std::optional<Ref<ClosedNet>>& net2,
                                   const std::pair<PortType, PortType>& portTypes);

    template <typename T>
    // try to call this with ports over nodes if you have the port
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
    [[nodiscard]] std::size_t getNodeConCount(const Ref<Node>& node) const;
    void                      remove();
};

using ObjAtCoordVar = std::variant<std::monostate, Connection, std::pair<Connection, Connection>,
                                   PortRef, Ref<Node>, Ref<Gate>, Ref<BlockInst>>;
enum struct ObjAtCoordType : std::size_t {
    Empty    = 0,
    Con      = 1,
    ConCross = 2,
    Port     = 3,
    Node     = 4,
    Gate     = 5,
    Block    = 6
};
inline static constexpr std::array<std::string, 7> ObjAtCoordStrings{
    "empty", "connection", "conn crossing", "port", "node", "gate", "block"}; // for debugging

inline ObjAtCoordType typeOf(const ObjAtCoordVar& ref) { return ObjAtCoordType{ref.index()}; }

struct Block {
    std::size_t size = 100;
    std::string name;
    std::string description;

    StableVector<BlockInst> blockInstances;
    StableVector<Node>      nodes;
    StableVector<Gate>      gates;
    ConnectionNetwork       conNet;
    std::vector<Port>       ports;

    PortInst&                     getPort(const PortRef& port);
    const PortInst&               getPort(const PortRef& port) const;
    PortType                      getPortType(const PortRef& port) const;
    std::pair<PortType, PortType> getPortType(const Connection& con) const;
    bool                  collisionCheck(const Connection& con, const sf::Vector2i& coord) const;
    void                  splitCon(const Connection& con, Ref<Node> node);
    void                  makeOverlapNodes(const Connection& con, Ref<ClosedNet> net);
    [[nodiscard]] PortRef makeNewPortRef(ObjAtCoordVar& var, const sf::Vector2i& pos,
                                         Direction dirIntoPort);
    ObjAtCoordVar         whatIsAtCoord(const sf::Vector2i& coord) const;
};