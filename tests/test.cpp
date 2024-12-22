#include <ranges>

#include "gtest/gtest.h"

#include "block/Block.hpp"
#include "details/StableVector.hpp"

// Block
class BlockTest : public testing::Test, public Block {
  public:
    BlockTest() : Block("test", 50) {}
};

TEST_F(BlockTest, constructor) {
    EXPECT_TRUE(nodes.empty());
    EXPECT_TRUE(nets.empty());
    EXPECT_TRUE(blockInstances.empty());
}

TEST_F(BlockTest, makeNewPortAtEmpty) {
    sf::Vector2i pos        = {21, 21};
    auto         emptyPoint = whatIsAtCoord(pos);
    EXPECT_EQ(ObjAtCoordType::Empty, typeOf(emptyPoint));
    auto newPort = makeNewPortRef(pos, Direction::up);
    EXPECT_EQ(PortObjType::Node, typeOf(newPort));
    EXPECT_EQ(Direction::up, Direction(newPort.portNum));
    auto nodePoint = whatIsAtCoord(pos);
    EXPECT_EQ(ObjAtCoordType::Node, typeOf(nodePoint));
    auto nodeRef = std::get<Ref<Node>>(nodePoint);
    EXPECT_EQ(nodeRef, std::get<Ref<Node>>(newPort.ref));
    EXPECT_EQ(nodes.size(), 1);
    EXPECT_TRUE(nodes.contains(nodeRef));
}

TEST_F(BlockTest, makeEmptyToEmptyCon) {
    sf::Vector2i pos        = {21, 21};
    auto         firstPort  = makeNewPortRef(pos, Direction::up);
    sf::Vector2i secondPos  = {21, 13};
    auto         secondPort = makeNewPortRef(secondPos, Direction::up); // wrong dir
    EXPECT_ANY_THROW(insertCon({firstPort, secondPort}));
    secondPort = makeNewPortRef(secondPos, Direction::down); // right dir
    Connection con{firstPort, secondPort};
    insertCon(con);
    auto net = getClosNetRef(con);
    ASSERT_TRUE(net.has_value());
    EXPECT_EQ(net.value(), getClosNetRef(firstPort).value());
    EXPECT_EQ(net.value(), getClosNetRef(secondPort).value());
    auto conPoint = whatIsAtCoord((secondPos + pos) / 2);
    EXPECT_EQ(ObjAtCoordType::Con, typeOf(conPoint));
    EXPECT_EQ(std::get<Connection>(conPoint), con);
}

TEST_F(BlockTest, makeNewPortAtNode) {
    sf::Vector2i startPos  = {0, 0};
    auto         startPort = makeNewPortRef(startPos, Direction::down);
    EXPECT_EQ(startPort, makeNewPortRef(startPos, Direction::down)); // not made yet so ok
    auto       endPort = makeNewPortRef({0, 10}, Direction::up);
    Connection con{startPort, endPort};
    insertCon(con);
    EXPECT_ANY_THROW(std::ignore = makeNewPortRef(startPos, Direction::down));
    auto       startPort2 = makeNewPortRef(startPos, Direction::right);
    auto       endPort2   = makeNewPortRef({10, 0}, Direction::left);
    Connection con2{startPort2, endPort2};
    insertCon(con2);
    EXPECT_EQ(nets.size(), 1);
    EXPECT_EQ(nodes.size(), 3);
    EXPECT_EQ(getNodeConCount(std::get<Ref<Node>>(startPort.ref)), 2);
    auto& net = *nets.begin();
    EXPECT_TRUE(net.obj.isConnected(endPort, endPort2));
}

TEST_F(BlockTest, removeRedundantNode) {
    auto con1 = addConnection({0, 0}, {0, 1});
    auto con2 = addConnection({0, 2}, {0, 3});
    addConnection({0, 1}, {0, 2});
    EXPECT_EQ(nodes.size(), 2);
    auto net = nets[getClosNetRef(con1.portRef1).value()];
    EXPECT_TRUE(net.isConnected(con1.portRef1, con2.portRef2));
    EXPECT_EQ(net.getSize(), 1);
}

TEST_F(BlockTest, splitConnection1) {
    Connection con1         = addConnection({0, 0}, {5, 0});
    PortRef    splitConPort = makeNewPortRef({2, 0}, Direction::down);
    auto&      net          = nets[getClosNetRef(con1.portRef1).value()];
    EXPECT_EQ(nodes.size(), 3);
    EXPECT_EQ(nets.size(), 1);
    EXPECT_EQ(net.getSize(), 2);
    EXPECT_FALSE(net.contains(con1));
    EXPECT_TRUE(
        net.contains({con1.portRef1, {splitConPort.ref, static_cast<size_t>(Direction::left)}}));
    EXPECT_TRUE(
        net.contains({con1.portRef2, {splitConPort.ref, static_cast<size_t>(Direction::right)}}));
    EXPECT_TRUE(net.isConnected(con1.portRef1, con1.portRef2));
}

TEST_F(BlockTest, splitConnection2) {
    Connection con1     = addConnection({0, 0}, {5, 0});
    Connection con2     = addConnection({0, 5}, {5, 5});
    Connection splitCon = addConnection({2, 0}, {2, 5});
    EXPECT_EQ(nodes.size(), 6);
    insertCon(splitCon);
    EXPECT_EQ(nets.size(), 1);
    EXPECT_TRUE(nets.begin()->obj.contains(splitCon));
    EXPECT_TRUE(nets.begin()->obj.isConnected(con1.portRef1, con2.portRef2));
}

TEST_F(BlockTest, overlapNode) {
    Connection con1 = addConnection({0, 2}, {5, 2});
    Connection con2 = addConnection({2, 0}, {2, 5});
    EXPECT_EQ(nets.size(), 2);
    EXPECT_EQ(nodes.size(), 4);
    insertOverlap(con1, con2, {2, 2});
    EXPECT_EQ(nets.size(), 1);
    EXPECT_EQ(nodes.size(), 5);
    auto& net      = nets.begin()->obj;
    auto  coordObj = whatIsAtCoord({2, 2});
    ASSERT_EQ(typeOf(coordObj), ObjAtCoordType::Node);
    auto node = std::get<Ref<Node>>(coordObj);
    EXPECT_TRUE(net.contains({con1.portRef1, {node, static_cast<std::size_t>(Direction::left)}}));
    EXPECT_TRUE(net.contains({con1.portRef2, {node, static_cast<std::size_t>(Direction::right)}}));
    EXPECT_TRUE(net.contains({con2.portRef1, {node, static_cast<std::size_t>(Direction::up)}}));
    EXPECT_TRUE(net.contains({con2.portRef2, {node, static_cast<std::size_t>(Direction::down)}}));
    EXPECT_TRUE(net.isConnected(con1.portRef1, con2.portRef2));
}

TEST_F(BlockTest, eraseConDelNodeAndNet) {
    Connection con1 = addConnection({0, 0}, {5, 0});
    eraseCon(con1);
    EXPECT_EQ(nodes.size(), 0);
    EXPECT_EQ(nets.size(), 0);
}

TEST_F(BlockTest, eraseConSplitNet1) {
    Connection con1   = addConnection({0, 0}, {5, 0});
    Connection con2   = addConnection({5, 0}, {5, 5});
    auto       netRef = getClosNetRef(con1).value();
    EXPECT_TRUE(nets[netRef].isConnected(con1.portRef1, con2.portRef2));
    EXPECT_EQ(nodes.size(), 3);
    EXPECT_EQ(nets.size(), 1);
    eraseCon(con2);
    EXPECT_EQ(nodes.size(), 2);
    EXPECT_EQ(nets.size(), 1);
    auto& net1 = nets[getClosNetRef(con1).value()];
    EXPECT_EQ(net1.getSize(), 1);
    EXPECT_TRUE(net1.contains(con1));
    EXPECT_FALSE(net1.contains(con2));
    EXPECT_FALSE(net1.isConnected(con1.portRef1, con2.portRef2));
}

TEST_F(BlockTest, eraseConSplitNet2) {
    Connection con1 = addConnection({0, 0}, {5, 0});
    Connection con2 = addConnection({0, 5}, {5, 5});
    Connection con3 = addConnection({5, 0}, {5, 5});
    auto&      net  = nets[getClosNetRef(con3).value()];
    EXPECT_TRUE(net.isConnected(con1.portRef1, con2.portRef2));
    EXPECT_EQ(nodes.size(), 4);
    EXPECT_EQ(nets.size(), 1);
    eraseCon(con3);
    EXPECT_EQ(nets.size(), 2);
    EXPECT_FALSE(getClosNetRef(con3).has_value());
    auto& net1 = nets[getClosNetRef(con1).value()];
    EXPECT_EQ(net1.getSize(), 1);
    auto& net2 = nets[getClosNetRef(con2).value()];
    EXPECT_EQ(net2.getSize(), 1);
}

TEST_F(BlockTest, eraseConSplitNet3) {
    Connection con1 = addConnection({0, 0}, {4, 0});
    Connection con2 = addConnection({0, 4}, {4, 4});
    Connection con3 = addConnection({2, 0}, {2, 4});
    // 1,2 and 3 make an I
    Connection con4 = addConnection({0, 2}, {4, 2});
    // con 4 makes puts a horizontal cut through the I
    insertOverlap(con4, con3, {2, 2});
    auto& net = nets.front().obj;
    EXPECT_EQ(nets.size(), 1);
    EXPECT_EQ(net.getSize(), 8);
    EXPECT_TRUE(net.isConnected(con1.portRef1, con2.portRef2));
    auto nodeCoord = whatIsAtCoord({2, 2});
    ASSERT_EQ(typeOf(nodeCoord), ObjAtCoordType::Node);
    auto node = std::get<Ref<Node>>(nodeCoord);
    eraseCon({con3.portRef1, {node, static_cast<std::size_t>(Direction::up)}});
    EXPECT_EQ(nets.size(), 2);
    auto newNetOpt = getClosNetRef(con1);
    ASSERT_TRUE(newNetOpt.has_value());
    EXPECT_TRUE(nets[newNetOpt.value()].contains(con1));
    // auto newNet = newNetOpt.value();
}

// StableVector
template <typename T, typename Q>
void equalityCheck(T subj, const std::vector<Q>& vec) { // subj is taken by value
    EXPECT_EQ(subj.size(), vec.size());
    for (auto v: vec) {
        EXPECT_TRUE(subj.contains(v.first));
        EXPECT_TRUE(subj[v.first] == v.second);
        subj.erase(v.first);
    }
    EXPECT_EQ(subj.size(), 0);
}

template <typename T>
class StableVectors : public testing::Test {
  public:
    T vec;
};

using TestedTypes = testing::Types<PepperedVector<int>, CompactMap<int>>;
TYPED_TEST_SUITE(StableVectors, TestedTypes);

TYPED_TEST(StableVectors, IsEmptyIntially) { EXPECT_TRUE(this->vec.empty()); }

TYPED_TEST(StableVectors, IterateEmpty) {
    auto& v = this->vec;
    EXPECT_EQ(v.begin(), v.end());
    for ([[maybe_unused]] auto& e: v) EXPECT_TRUE(false);
    for ([[maybe_unused]] const auto& e: v) EXPECT_TRUE(false);
    auto ref = v.insert(0);
    v.erase(ref);
    ASSERT_TRUE(v.empty());
    EXPECT_EQ(v.begin(), v.end());
    for ([[maybe_unused]] auto& e: v) EXPECT_TRUE(false);
    for ([[maybe_unused]] const auto& e: v) EXPECT_TRUE(false);
}

TYPED_TEST(StableVectors, Adding) {
    auto& v     = this->vec;
    auto  newItem = v.insert(0);
    EXPECT_TRUE(v.contains(newItem));
    EXPECT_EQ(v[newItem], 0);
    EXPECT_EQ(v.size(), 1);
    EXPECT_FALSE(v.empty());

    std::vector<std::pair<decltype(newItem), int>> curr{{newItem, 0}};
    for (auto i: std::views::iota(1, 10)) {
        curr.emplace_back(v.insert(i), i);
    }
    equalityCheck(v, curr);
}

TYPED_TEST(StableVectors, Iterating) {
    auto& v = this->vec;
    for (auto i: std::views::iota(0, 10)) v.insert(i);

    int sum{};
    for (const auto& i: v) {
        EXPECT_EQ(i.obj, v[i.ind]);
        sum += i.obj;
    }
    EXPECT_EQ(sum, 45);
}

TYPED_TEST(StableVectors, RemovingSimple) {
    auto& v     = this->vec;
    auto  newItem = v.insert(0);
    v.erase(newItem);
    EXPECT_FALSE(v.contains(newItem));
    EXPECT_EQ(v.size(), 0);
    newItem = v.insert(0);
    EXPECT_EQ(v.size(), 1);
    EXPECT_TRUE(v.contains(newItem));
    auto newItem2 = v.insert(1);
    EXPECT_EQ(v.size(), 2);
    EXPECT_TRUE(v.contains(newItem));
    EXPECT_TRUE(v.contains(newItem2));
}

TYPED_TEST(StableVectors, RemovingAndAdding) {
    auto& v     = this->vec;
    auto  newItem = v.insert(0);
    v.erase(newItem);

    std::vector<std::pair<decltype(newItem), int>> curr{};
    for (auto i: std::views::iota(0, 10)) {
        curr.emplace_back(v.insert(i), i);
    }
    equalityCheck(v, curr); // curr = 0,1,2,3,4,5,6,7,8,9

    for (auto i: std::vector<std::size_t>{7, 4, 3, 5, 5}) {
        v.erase(curr.at(i).first);
        curr.erase(curr.begin() + static_cast<signed long long>(i));
    }
    equalityCheck(v, curr); // curr = 0,1,2,5,6

    for (auto i: std::views::iota(10, 15)) {
        curr.emplace_back(v.insert(i), i);
    }
    equalityCheck(v, curr); // curr = 0,1,2,5,6,10,11,12,13,14

    std::vector<decltype(newItem)> removes;
    for (auto i: std::vector<std::size_t>{9, 0, 3, 2, 5}) {
        removes.push_back(curr.at(i).first);
        curr.erase(curr.begin() + static_cast<long long int>(i));
    }
    v.erase(removes);
    equalityCheck(v, curr); // cur = 1,2,10,11,12

    for (int i: std::views::iota(0, 201)) {
        curr.emplace_back(v.insert(i), i);
    }
    equalityCheck(v, curr);

    int sum = std::accumulate(v.begin(), v.end(), 0,
                              [](auto total, const auto& e) { return total += e.obj; });
    EXPECT_EQ(sum, 20136);

    removes = {};
    for (auto i: curr) {
        removes.push_back(i.first);
    }
    v.erase(removes);
    EXPECT_EQ(v.size(), 0);
}
