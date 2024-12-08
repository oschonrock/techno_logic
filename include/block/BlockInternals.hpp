#pragma once

#include "Helpers.hpp"
#include "absl/container/flat_hash_set.h"
#include "details/StableVector.hpp"

enum struct PortType { input, output, node };

struct PortInst {
    Direction    portDir;
    sf::Vector2i portPos;
    // PortType     portType;
    bool negated = false;
};

struct Node {
    sf::Vector2i            pos;
    std::array<PortInst, 4> ports;

    // ALWAYS form a connection to node after construction
    Node(const sf::Vector2i& pos_) : pos(pos_) {
        ports[0] = {Direction::up, pos};
        ports[1] = {Direction::down, pos};
        ports[2] = {Direction::left, pos};
        ports[3] = {Direction::right, pos};
    }
};

struct Gate {
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
};

class Block;

struct Port {
    std::string name;
    PortType    type;
};

struct BlockInst {
    sf::Vector2i          pos;
    std::vector<PortInst> ports;
    Ref<Block>            block;
};

using PortObjRef = std::variant<Ref<Node>, Ref<Gate>, Ref<BlockInst>>;
enum struct PortObjType : std::size_t { Node = 0, Gate = 1, BlockInst = 2 };
static constexpr std::array<std::string, 3> PortObjRefStrings{"node", "gate", "block"};
inline PortObjType typeOf(const PortObjRef& ref) { return PortObjType(ref.index()); }

struct PortRef {
    PortObjRef  ref;
    std::size_t portNum;
    PortRef(PortObjRef ref_, std::size_t portNum_) : ref(ref_), portNum(portNum_) {}

    bool operator==(const PortRef& other) const {
        return (ref == other.ref) && (portNum == other.portNum);
    }
};

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
    bool isConnected(const PortRef& start, const PortRef& end,
                     absl::flat_hash_set<PortRef>& alreadyChecked) const {
        if (start == end) return true;
        if (alreadyChecked.contains(start)) return false;
        alreadyChecked.insert(start);
        if (typeOf(start) != PortObjType::Node) { // if start not node
            if (!contains(start))
                return false;
            else if (isConnected(getCon(start).portRef2, end, alreadyChecked))
                return true;
        } else if (typeOf(start) == PortObjType::Node) { // if start node
            for (std::size_t portNum = 0; portNum < 4; ++portNum) {
                PortRef nodePort{start.ref, portNum};
                if (nodePort == end) return true;
                alreadyChecked.insert(nodePort);
                if (contains(nodePort) &&
                    isConnected(getCon(nodePort).portRef2, end, alreadyChecked)) {
                    return true;
                }
            }
        }
        return false;
    }

    // void addConnected(const PortRef& curr, ClosedNet& newNet) { newNet.insert(curr) }

  public:
    [[nodiscard]] bool                          hasInput() const { return input.has_value(); }
    [[nodiscard]] const std::optional<PortRef>& getInput() const { return input; }
    [[nodiscard]] const std::vector<PortRef>&   getOutputs() const { return outputs; }
    [[nodiscard]] std::size_t                   getSize() const { return size; }

    void insert(const Connection& con, const std::pair<PortType, PortType>& portTypes);
    void erase(const Connection& con, const std::pair<PortType, PortType>& portTypes);
    [[nodiscard]] bool contains(const PortRef& port) const;
    [[nodiscard]] bool contains(const Connection& con) const;
    // prefer call contains() on port
    [[nodiscard]] bool contains(const Ref<Node> node) const;
    bool               isConnected(const PortRef& start, const PortRef& end) const {
        if (!contains(start) || !contains(end)) return false;
        absl::flat_hash_set<PortRef> alreadyChecked{};
        return isConnected(start, end, alreadyChecked);
    }
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
        using iterator_category = std::input_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = Connection;
        using pointer           = const Connection*; // or also value_type*
        using reference         = const Connection&; // or also value_type&

        using MapIt = absl::flat_hash_map<PortRef, PortRef>::const_iterator;
        Iterator()  = delete;
        Iterator(MapIt it, MapIt end) : it_(it), end_(end), con(it_->first, it_->second) {}
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