#include "Block.hpp"

Node::Node(const sf::Vector2i& pos_) : pos(pos_) {
    ports[0] = {Direction::up, pos};
    ports[1] = {Direction::down, pos};
    ports[2] = {Direction::left, pos};
    ports[3] = {Direction::right, pos};
}

void ClosedNet::maintainIOVecs(bool isInsert, const PortRef& portRef) {
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

void ClosedNet::insert(const Connection& con) {
    conMap.insert({con.portRef1, con.portRef2});
    conMap2.insert({con.portRef2, con.portRef1});
    ++size;
    maintainIOVecs(true, con.portRef1);
    maintainIOVecs(true, con.portRef2);
}

void ClosedNet::erase(const Connection& con) {
    conMap.erase(con.portRef1);
    conMap.erase(con.portRef2);
    conMap2.erase(con.portRef1);
    conMap2.erase(con.portRef2);
    --size;
    maintainIOVecs(false, con.portRef1);
    maintainIOVecs(false, con.portRef2);
}

[[nodiscard]] bool ClosedNet::contains(const PortRef& port) const {
    return conMap.contains(port) || conMap2.contains(port);
}

[[nodiscard]] bool ClosedNet::contains(const Connection& con) const {
    return (conMap.contains(con.portRef1) && conMap.find(con.portRef1)->second == con.portRef2) ||
           (conMap.contains(con.portRef2) && conMap.find(con.portRef2)->second == con.portRef1);
}

// prefer call contains() on port
[[nodiscard]] bool ClosedNet::contains(const Ref<Node> node) const {
    return contains(PortRef{node, 0, PortType::node}) ||
           contains(PortRef{node, 1, PortType::node}) ||
           contains(PortRef{node, 2, PortType::node}) || contains(PortRef{node, 3, PortType::node});
}

[[nodiscard]] Connection ClosedNet::getCon(const PortRef& port) const {
    if (conMap.contains(port)) {
        return Connection{port, conMap.find(port)->second};
    } else if (conMap2.contains(port)) {
        return Connection{port, conMap2.find(port)->second};
    }
    throw std::logic_error("Port not connected to closed net. Did you call contains?");
}

// void ConnectionNetwork::copyConnectedPorts(Block& block, const ClosedNet& sourceNet, ClosedNet&
// destNet, const
//     PortRef& port) {
//         if (destNet.contains(port)) return;
//         if (block.getPort());
//     }

void ConnectionNetwork::insert(const Connection& con, const std::optional<Ref<ClosedNet>>& net1,
                               const std::optional<Ref<ClosedNet>>& net2) {
    if (!net1 && !net2) { // make new closed network
        auto netRef = nets.insert(ClosedNet{});
        nets[netRef].insert(con);
        return;
    }
    if (net1 && net2) { // connecting two existing networks
        auto net1Ref = net1.value();
        auto net2Ref = net2.value();
        if (net1Ref == net2Ref) { // making loop within closed network
            nets[net1Ref].insert(con);
            return;
        } else { // connecting two closed networks
            if (nets[net1Ref].getSize() < nets[net2Ref].getSize()) std::swap(net1Ref, net2Ref);
            // net1 is now the bigger of the two
            nets[net1Ref] += nets[net2Ref];
            nets.erase(net2Ref);
            nets[net1Ref].insert(con);
            return;
        }
    }
    // extending network
    nets[net1 ? net1.value() : net2.value()].insert(con);
    return;
}

[[nodiscard]] std::size_t ConnectionNetwork::getNodeConCount(const Ref<Node>& node) const {
    const auto& net   = nets[getClosNetRef(node).value()]; // connection assumed because node
    std::size_t count = 0;
    for (std::size_t port = 0; port < 4; ++port) {
        if (net.contains(PortRef{node, port, PortType::node})) ++count;
    }
    return count;
}

PortInst& Block::getPort(const PortRef& port) {
    switch (typeOf(port.ref)) {
    case PortObjType::Node:
        return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case PortObjType::Gate:
        return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case PortObjType::BlockInst:
        return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
}
const PortInst& Block::getPort(const PortRef& port) const {
    switch (typeOf(port.ref)) {
    case PortObjType::Node:
        return nodes[std::get<Ref<Node>>(port.ref)].ports[port.portNum];
    case PortObjType::Gate:
        return gates[std::get<Ref<Gate>>(port.ref)].ports[port.portNum];
    case PortObjType::BlockInst:
        return blockInstances[std::get<Ref<BlockInst>>(port.ref)].ports[port.portNum];
    }
}

bool Block::collisionCheck(const Connection& con, const sf::Vector2i& coord) const {
    return isVecBetween(coord, getPort(con.portRef1).portPos, getPort(con.portRef2).portPos);
}