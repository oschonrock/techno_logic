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

    void maintainIOVecs(bool isInsert, const PortRef& portRef, const PortType& portType) {
        if (portType == PortType::node) return;
        if (portType == PortType::input) {
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
    bool isConnected(const PortRef& start, const PortRef& end,
                     absl::flat_hash_set<PortRef>& alreadyChecked) const {
        if (start == end) return true;
        if (alreadyChecked.contains(start)) return false;
        alreadyChecked.insert(start);
        if (typeOf(start) != PortObjType::Node) { // if start not node
            return contains(start) && isConnected(getCon(start).portRef2, end, alreadyChecked);
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

    void stealConnetedCons(const PortRef& currentPort, ClosedNet& newNet) {
        if (contains(currentPort) && !newNet.contains(currentPort)) {
            auto currentCon = getCon(currentPort);
            // don't update input/output for now
            newNet.insert(currentCon, {PortType::node, PortType::node});
            erase(currentCon, {PortType::node, PortType::node});
            stealConnetedCons(currentCon.portRef2, newNet);
        }
        if (typeOf(currentPort) == PortObjType::Node) {
            for (std::size_t portNum = 0; portNum < 4; ++portNum) {
                PortRef nodePort{currentPort.ref, portNum};
                if (contains(nodePort) && !newNet.contains(nodePort)) {
                    auto currentCon = getCon(nodePort);
                    // don't update input/output for now
                    newNet.insert(currentCon, {PortType::node, PortType::node});
                    erase(currentCon, {PortType::node, PortType::node});
                    stealConnetedCons(currentCon.portRef2, newNet);
                }
            }
        }
    }

    // void addConnected(const PortRef& curr, ClosedNet& newNet) { newNet.insert(curr) }

  public:
    [[nodiscard]] bool                          hasInput() const { return input.has_value(); }
    [[nodiscard]] const std::optional<PortRef>& getInput() const { return input; }
    [[nodiscard]] const std::vector<PortRef>&   getOutputs() const { return outputs; }
    [[nodiscard]] std::size_t                   getSize() const { return size; }

    void insert(const Connection& con, const std::pair<PortType, PortType>& portTypes) {
        conMap.insert({con.portRef1, con.portRef2});
        conMap2.insert({con.portRef2, con.portRef1});
        ++size;
        maintainIOVecs(true, con.portRef1, portTypes.first);
        maintainIOVecs(true, con.portRef2, portTypes.second);
    }

    void erase(const Connection& con, const std::pair<PortType, PortType>& portTypes) {
        conMap.erase(con.portRef1);
        conMap.erase(con.portRef2);
        conMap2.erase(con.portRef1);
        conMap2.erase(con.portRef2);
        --size;
        maintainIOVecs(false, con.portRef1, portTypes.first);
        maintainIOVecs(false, con.portRef2, portTypes.second);
    }

    ClosedNet splitNet(const PortRef& startPort) {
        ClosedNet newNet{};
        stealConnetedCons(startPort, newNet);
        outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
                                     [&](auto& output) {
                                         if (newNet.contains(output)) { // output has been copied
                                             newNet.outputs.push_back(output);
                                             return true;
                                         }
                                         return false;
                                     }),
                      outputs.end());
        if (input.has_value() && newNet.contains(input.value())) {
            newNet.input = input;
            input.reset(); // input has been copied
        }
        return newNet;
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
        return contains(PortRef{node, 0}) || contains(PortRef{node, 1}) ||
               contains(PortRef{node, 2}) || contains(PortRef{node, 3});
    }
    bool isConnected(const PortRef& start, const PortRef& end) const {
        // if (!contains(start) || !contains(end)) return false;
        absl::flat_hash_set<PortRef> alreadyChecked{};
        return isConnected(start, end, alreadyChecked);
    }
    [[nodiscard]] Connection getCon(const PortRef& port) const {
        if (conMap.contains(port)) {
            return Connection{port, conMap.find(port)->second};
        } else if (conMap2.contains(port)) {
            return Connection{port, conMap2.find(port)->second};
        }
        throw std::logic_error("Port not connected to closed net. Did you call contains?");
    }

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
        Iterator(MapIt it, MapIt end) : it_(it), end_(end) {
            if (it_ != end_) con = Connection(it_->first, it_->second);
        }

        [[nodiscard]] reference operator*() const {
            if (it_ == end_) throw std::out_of_range("Tried to derefence end");
            return con.value();
        }
        [[nodiscard]] pointer operator->() const {
            if (it_ == end_) throw std::out_of_range("Tried to derefence end");
            return &con.value();
        }

        Iterator& operator++() { // Prefix increment
            ++it_;
            if (it_ != end_)
                con = Connection(it_->first, it_->second);
            else
                con = {};
            return *this;
        }

        Iterator operator++(int) { // Postfix increment
            Iterator tmp = *this;
            ++*this;
            return tmp;
        }

        friend bool operator==(const Iterator& a, const Iterator& b) { return a.it_ == b.it_; }
        friend bool operator!=(const Iterator& a, const Iterator& b) { return a.it_ != b.it_; }

      private:
        MapIt                     it_;
        MapIt                     end_; // required to avoid dereference of end()
        std::optional<Connection> con;
    };

    static_assert(std::input_iterator<Iterator>);
    [[nodiscard]] Iterator begin() const { return Iterator(conMap.begin(), conMap.end()); }
    [[nodiscard]] Iterator end() const { return Iterator(conMap.end(), conMap.end()); }
};