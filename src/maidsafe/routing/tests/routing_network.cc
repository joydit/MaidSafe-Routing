/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#include "maidsafe/routing/tests/routing_network.h"

#include <future>
#include <set>
#include <string>

#include "maidsafe/common/log.h"
#include "maidsafe/common/node_id.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/routing/routing_impl.h"
#include "maidsafe/routing/return_codes.h"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/tests/test_utils.h"
#include "maidsafe/routing/routing_pb.h"

namespace asio = boost::asio;
namespace ip = asio::ip;

namespace maidsafe {

namespace routing {

namespace test {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

size_t GenericNode::next_node_id_(1);

GenericNode::GenericNode(bool client_mode)
    : functors_(),
      id_(0),
      node_info_plus_(std::make_shared<NodeInfoAndPrivateKey>(MakeNodeInfoAndKeys())),
      routing_(),
      mutex_(),
      client_mode_(client_mode),
      anonymous_(false),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      endpoint_(),
      messages_() {
  endpoint_.address(maidsafe::GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  functors_.close_node_replaced = nullptr;
  functors_.message_received = [&] (const std::string& message,
                                    const NodeId&,
                                    ReplyFunctor reply_functor) {
                                 LOG(kInfo) << id_ << " -- Received: message : "
                                            << message.substr(0, 10);
                                 std::lock_guard<std::mutex> guard(mutex_);
                                 messages_.push_back(message);
                                 if (!IsClient())
                                   reply_functor("Response to >:<" + message);
                               };
  functors_.network_status = nullptr;
  routing_.reset(new Routing(GetFob(*node_info_plus_), client_mode));
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(bool client_mode, const rudp::NatType& nat_type)
    : functors_(),
      id_(0),
      node_info_plus_(std::make_shared<NodeInfoAndPrivateKey>(MakeNodeInfoAndKeys())),
      routing_(),
      mutex_(),
      client_mode_(client_mode),
      anonymous_(false),
      joined_(false),
      expected_(0),
      nat_type_(nat_type),
      endpoint_(),
      messages_() {
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  functors_.close_node_replaced = nullptr;
  functors_.message_received = [&] (const std::string& message,
                                    const NodeId&,
                                    ReplyFunctor reply_functor) {
                                 LOG(kInfo) << id_ << " -- Received: message : "
                                            << message.substr(0, 10);
                                 std::lock_guard<std::mutex> guard(mutex_);
                                 messages_.push_back(message);
                                 if (!IsClient())
                                   reply_functor("Response to >:<" + message);
                               };
  functors_.network_status = nullptr;
  routing_.reset(new Routing(GetFob(*node_info_plus_), client_mode));
  routing_->pimpl_->network_.nat_type_ = nat_type_;
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}

GenericNode::GenericNode(bool client_mode, const NodeInfoAndPrivateKey& node_info)
    : functors_(),
      id_(0),
      node_info_plus_(std::make_shared<NodeInfoAndPrivateKey>(node_info)),
      routing_(),
      mutex_(),
      client_mode_(client_mode),
      anonymous_(false),
      joined_(false),
      expected_(0),
      nat_type_(rudp::NatType::kUnknown),
      endpoint_(),
      messages_() {
  endpoint_.address(GetLocalIp());
  endpoint_.port(maidsafe::test::GetRandomPort());
  functors_.close_node_replaced = nullptr;
  functors_.message_received = [&] (const std::string& message,
                                    const NodeId&,
                                    ReplyFunctor reply_functor) {
                                 LOG(kInfo) << id_ << " -- Received: message : "
                                            << message.substr(0, 10);
                                 std::lock_guard<std::mutex> guard(mutex_);
                                 messages_.push_back(message);
                                 if (!IsClient())
                                   reply_functor("Response to >:<" + message);
                               };
  functors_.network_status = nullptr;
  Fob fob(GetFob(*node_info_plus_));
  if (node_info_plus_->node_info.node_id.IsZero()) {
    anonymous_ = true;
    fob.identity = Identity();
  }
  routing_.reset(new Routing(fob, client_mode));
  LOG(kVerbose) << "Node constructor";
  std::lock_guard<std::mutex> lock(mutex_);
  id_ = next_node_id_++;
}


GenericNode::~GenericNode() {}

int GenericNode::GetStatus() const {
  return /*routing_->GetStatus()*/0;
}

Endpoint GenericNode::endpoint() const {
  return endpoint_;
}

NodeId GenericNode::connection_id() const {
  return node_info_plus_->node_info.connection_id;
}

NodeId GenericNode::node_id() const {
  return node_info_plus_->node_info.node_id;
}

size_t GenericNode::id() const {
  return id_;
}

bool GenericNode::IsClient() const {
  return client_mode_;
}

void GenericNode::set_client_mode(const bool& client_mode) {
  client_mode_ = client_mode;
}

std::vector<NodeInfo> GenericNode::RoutingTable() const {
  return routing_->pimpl_->routing_table_.nodes_;
}

NodeId GenericNode::GetRandomExistingNode() const {
  return routing_->GetRandomExistingNode();
}

void GenericNode::AddNodeToRandomNodeHelper(const NodeId& node_id) {
  routing_->pimpl_->random_node_helper_.Add(node_id);
}

void GenericNode::RemoveNodeFromRandomNodeHelper(const NodeId& node_id) {
  routing_->pimpl_->random_node_helper_.Remove(node_id);
}

void GenericNode::Send(const NodeId& destination_id,
                       const NodeId& group_claim,
                       const std::string& data,
                       const ResponseFunctor& response_functor,
                       const boost::posix_time::time_duration& timeout,
                       bool direct,
                       bool cache) {
    routing_->Send(destination_id, group_claim, data, response_functor, timeout, direct, cache);
}

void GenericNode::RudpSend(const NodeId& peer_node_id,
                           const protobuf::Message& message,
                           rudp::MessageSentFunctor message_sent_functor) {
  routing_->pimpl_->network_.RudpSend(peer_node_id, message, message_sent_functor);
}

void GenericNode::SendToClosestNode(const protobuf::Message& message) {
  routing_->pimpl_->network_.SendToClosestNode(message);
}

bool GenericNode::RoutingTableHasNode(const NodeId& node_id) {
  return std::find_if(routing_->pimpl_->routing_table_.nodes_.begin(),
                      routing_->pimpl_->routing_table_.nodes_.end(),
                      [node_id](const NodeInfo& node_info) {
                        return node_id == node_info.node_id;
                      }) !=
         routing_->pimpl_->routing_table_.nodes_.end();
}

bool GenericNode::NonRoutingTableHasNode(const NodeId& node_id) {
  return std::find_if(routing_->pimpl_->non_routing_table_.nodes_.begin(),
                      routing_->pimpl_->non_routing_table_.nodes_.end(),
                      [&node_id](const NodeInfo& node_info) {
                        return (node_id == node_info.node_id);
                      }) !=
         routing_->pimpl_->non_routing_table_.nodes_.end();
}

testing::AssertionResult GenericNode::DropNode(const NodeId& node_id) {
  LOG(kInfo) << " DropNode " << HexSubstr(routing_->pimpl_->routing_table_.kNodeId_.string())
             << " Removes " << HexSubstr(node_id.string());
  auto iter = std::find_if(routing_->pimpl_->routing_table_.nodes_.begin(),
                           routing_->pimpl_->routing_table_.nodes_.end(),
                           [&node_id] (const NodeInfo& node_info) {
                             return (node_id == node_info.node_id);
                           });
  if (iter != routing_->pimpl_->routing_table_.nodes_.end()) {
    LOG(kVerbose) << HexSubstr(routing_->pimpl_->routing_table_.kNodeId_.string())
                  << " Removes " << HexSubstr(node_id.string());
//    routing_->pimpl_->network_.Remove(iter->connection_id);
    routing_->pimpl_->routing_table_.DropNode(iter->connection_id, false);
  } else {
    testing::AssertionFailure() << HexSubstr(routing_->pimpl_->routing_table_.fob_.identity)
                                << " does not have " << HexSubstr(node_id.string())
                                << " in routing table of ";
  }
  return testing::AssertionSuccess();
}


NodeInfo GenericNode::node_info() const {
  return node_info_plus_->node_info;
}

int GenericNode::ZeroStateJoin(const Endpoint& peer_endpoint, const NodeInfo& peer_node_info) {
  return routing_->ZeroStateJoin(functors_, endpoint(), peer_endpoint, peer_node_info);
}

void GenericNode::Join(const std::vector<Endpoint>& peer_endpoints) {
  routing_->Join(functors_, peer_endpoints);
}

void GenericNode::set_joined(const bool node_joined) {
  joined_ = node_joined;
}

bool GenericNode::joined() const {
  return joined_;
}

int GenericNode::expected() {
  return expected_;
}

void GenericNode::set_expected(const int& expected) {
  expected_ = expected;
}

void GenericNode::PrintRoutingTable() {
  LOG(kVerbose) << " PrintRoutingTable of "
                << HexSubstr(node_info_plus_->node_info.node_id.string())
                << (IsClient() ? " Client" : "Vault");
  for (auto node_info : routing_->pimpl_->routing_table_.nodes_) {
    LOG(kVerbose) << "NodeId: " << HexSubstr(node_info.node_id.string());
  }
  LOG(kVerbose) << "Non-RoutingTable of " << HexSubstr(node_info_plus_->node_info.node_id.string());
  for (auto node_info : routing_->pimpl_->non_routing_table_.nodes_) {
    LOG(kVerbose) << "NodeId: " << HexSubstr(node_info.node_id.string());
  }
}

size_t GenericNode::MessagesSize() const { return messages_.size(); }

void GenericNode::ClearMessages() {
  std::lock_guard<std::mutex> lock(mutex_);
  messages_.clear();
}

Fob GenericNode::fob() {
  std::lock_guard<std::mutex> lock(mutex_);
  return GetFob(*node_info_plus_);
}

GenericNetwork::GenericNetwork()
    : nodes_(),
      mutex_(),
      bootstrap_endpoints_(),
      bootstrap_path_("bootstrap"),
      fobs_(),
      client_index_(0) { LOG(kVerbose) << "RoutingNetwork Constructor"; }

GenericNetwork::~GenericNetwork() {
  std::lock_guard<std::mutex> lock(mutex_);
  fobs_.clear();
}

void GenericNetwork::SetUp() {
  NodePtr node1(new GenericNode(false)), node2(new GenericNode(false));
  nodes_.push_back(node1);
  nodes_.push_back(node2);
  client_index_ = 2;
  fobs_.push_back(node1->fob());
  fobs_.push_back(node2->fob());
  SetNodeValidationFunctor(node1);
  SetNodeValidationFunctor(node2);
  LOG(kVerbose) << "Setup started";
  auto f1 = std::async(std::launch::async, [=, &node2] ()->int {
    return node1->ZeroStateJoin(node2->endpoint(), node2->node_info());
  });
  auto f2 = std::async(std::launch::async, [=, &node1] ()->int {
    return node2->ZeroStateJoin(node1->endpoint(), node1->node_info());
  });
  EXPECT_EQ(kSuccess, f2.get());
  EXPECT_EQ(kSuccess, f1.get());
  LOG(kVerbose) << "Setup succeeded";
  bootstrap_endpoints_.clear();
  bootstrap_endpoints_.push_back(node1->endpoint());
  bootstrap_endpoints_.push_back(node2->endpoint());
}

void GenericNetwork::TearDown() {
  std::lock_guard<std::mutex> loch(mutex_);
  GenericNode::next_node_id_ = 1;
  for (auto &node : nodes_) {
    node->functors_.network_status = nullptr;
  }
  nodes_.clear();
}

void GenericNetwork::SetUpNetwork(const size_t& non_client_size, const size_t& client_size) {
  for (size_t index = 2; index < non_client_size; ++index) {
    NodePtr node(new GenericNode(false));
    AddNodeDetails(node);
    LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//      node->PrintRoutingTable();
  }
  for (size_t index = 0; index < client_size; ++index) {
    NodePtr node(new GenericNode(true));
    AddNodeDetails(node);
    LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
  }
  Sleep(boost::posix_time::seconds(1));
  PrintRoutingTables();
//    EXPECT_TRUE(ValidateRoutingTables());
}

void GenericNetwork::AddNode(const bool& client_mode, const NodeId& node_id, bool anonymous) {
  NodeInfoAndPrivateKey node_info;
  if (!anonymous) {
    node_info = MakeNodeInfoAndKeys();
    if (node_id != NodeId())
      node_info.node_info.node_id = node_id;
  }
  NodePtr node(new GenericNode(client_mode, node_info));
  AddNodeDetails(node);
  LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//    node->PrintRoutingTable();
}

void GenericNetwork::AddNode(const bool& client_mode, const rudp::NatType& nat_type) {
  NodeInfoAndPrivateKey node_info(MakeNodeInfoAndKeys());
  NodePtr node(new GenericNode(client_mode, nat_type));
  AddNodeDetails(node);
  LOG(kVerbose) << "Node # " << nodes_.size() << " added to network";
//    node->PrintRoutingTable();
}

bool GenericNetwork::RemoveNode(const NodeId& node_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = std::find_if(nodes_.begin(),
                           nodes_.end(),
                           [&node_id] (const NodePtr node) { return node_id == node->node_id(); });  // NOLINT (Dan)
  if (iter == nodes_.end())
    return false;

  if (!(*iter)->IsClient())
    --client_index_;
  nodes_.erase(iter);

  return true;
}

void GenericNetwork::Validate(const NodeId& node_id, GivePublicKeyFunctor give_public_key) {
  if (node_id == NodeId())
    return;
  std::lock_guard<std::mutex> lock(mutex_);

  auto iter(fobs_.begin());
  bool find(false);
  while ((iter != fobs_.end()) && !find) {
    if (iter->identity.string() == node_id.string())
      find = true;
    else
      ++iter;
  }
//  auto iter = std::find_if(nodes_.begin(),
//                           nodes_.end(),
//                           [&node_id] (const NodePtr& node)->bool {
//                             EXPECT_FALSE(GetKeys(*node->node_info_plus_).identity.empty());
//                             return GetKeys(*node->node_info_plus_).identity == node_id.string();
//                           });
  if (!fobs_.empty())
    EXPECT_NE(iter, fobs_.end());
  if (iter != fobs_.end())
    give_public_key((*iter).keys.public_key);
}

void GenericNetwork::SetNodeValidationFunctor(NodePtr node) {
  node->functors_.request_public_key = [this] (const NodeId& node_id,
                                               GivePublicKeyFunctor give_public_key) {
                                         this->Validate(node_id, give_public_key);
                                       };
}

void GenericNetwork::PrintRoutingTables() {
  std::lock_guard<std::mutex> loch(mutex_);
  for (auto node : nodes_)
    node->PrintRoutingTable();
}

bool GenericNetwork::ValidateRoutingTables() {
  std::vector<NodeId> node_ids;
  for (auto node : nodes_) {
    if (!node->IsClient())
      node_ids.push_back(node->node_id());
  }
  for (auto node : nodes_) {
    LOG(kVerbose) << "Reference node: " << HexSubstr(node->node_id().string());
    std::sort(node_ids.begin(),
              node_ids.end(),
              [=] (const NodeId& lhs, const NodeId& rhs)->bool {
                return NodeId::CloserToTarget(lhs, rhs, node->node_id());
              });
    for (auto node_id : node_ids)
      LOG(kVerbose) << HexSubstr(node_id.string());
//      node->PrintRoutingTable();
    auto routing_table(node->RoutingTable());
//      EXPECT_FALSE(routing_table.size() < Parameters::closest_nodes_size);
    std::sort(routing_table.begin(),
              routing_table.end(),
              [&, this] (const NodeInfo& lhs, const NodeInfo& rhs)->bool {
                return NodeId::CloserToTarget(lhs.node_id, rhs.node_id, node->node_id());
              });
    LOG(kVerbose) << "Print ordered RT";
    uint16_t size(std::min(static_cast<uint16_t>(routing_table.size()),
                           Parameters::closest_nodes_size));
    for (auto node_info : routing_table)
      LOG(kVerbose) << HexSubstr(node_info.node_id.string());
    for (auto iter(routing_table.begin());
         iter < routing_table.begin() + size -1;
         ++iter) {
      size_t distance(std::distance(node_ids.begin(), std::find(node_ids.begin(),
                                                                node_ids.end(),
                                                                (*iter).node_id)));
       LOG(kVerbose) << "distance: " << distance << " from "
                     << HexSubstr((*iter).node_id.string());
       if (distance > size)
        return false;
    }
  }
  return true;
}

GenericNetwork::NodePtr GenericNetwork::RandomClientNode() {
  std::lock_guard<std::mutex> loch(mutex_);
  size_t client_count(nodes_.size() - client_index_);
  NodePtr random(nodes_.at((RandomUint32() % client_count) + client_index_));
  return random;
}

GenericNetwork::NodePtr GenericNetwork::RandomVaultNode() {
  std::lock_guard<std::mutex> loch(mutex_);
  NodePtr random(nodes_.at(RandomUint32() % client_index_));
  return random;
}

void GenericNetwork::RemoveRandomClient() {
  std::lock_guard<std::mutex> loch(mutex_);
  size_t client_count(nodes_.size() - client_index_);
  nodes_.erase(nodes_.begin() + ((RandomUint32() % client_count) + client_index_));
}

void GenericNetwork::RemoveRandomVault() {
  std::lock_guard<std::mutex> loch(mutex_);
  assert(nodes_.size() > 2);
  assert(client_index_ > 2);
  nodes_.erase(nodes_.begin() + 2 + (RandomUint32() % client_index_));  // +2 to avoid zero state
  --client_index_;
}

uint16_t GenericNetwork::NonClientNodesSize() const {
  uint16_t non_client_size(0);
  for (auto node : nodes_) {
    if (!node->IsClient())
      non_client_size++;
  }
  return non_client_size;
}

void GenericNetwork::AddNodeDetails(NodePtr node) {
  std::condition_variable cond_var;
  std::mutex mutex;
  {
    std::lock_guard<std::mutex> loch(mutex_);
    fobs_.push_back(node->fob());
    SetNodeValidationFunctor(node);
    uint16_t node_size(NonClientNodesSize());
    node->set_expected(NetworkStatus(node->IsClient(),
                                     std::min(node_size, Parameters::closest_nodes_size)));
    if (node->IsClient()) {
      nodes_.push_back(node);
    } else {
      nodes_.insert(nodes_.begin() + client_index_, node);
      ++client_index_;
    }
  }
  std::weak_ptr<GenericNode> weak_node(node);
  node->functors_.network_status =
      [&cond_var, weak_node] (const int& result) {
        if (NodePtr node = weak_node.lock()) {
          if (!node->anonymous_) {
            ASSERT_GE(result, kSuccess);
          } else  {
            if (!node->joined()) {
              ASSERT_EQ(result, kSuccess);
            } else if (node->joined()) {
              ASSERT_EQ(result, kAnonymousSessionEnded);
            }
          }
          if ((result == node->expected() && !node->joined()) || node->anonymous_) {
            node->set_joined(true);
            cond_var.notify_one();
          }
        }
      };
  node->Join(bootstrap_endpoints_);

  if (!node->joined()) {
    std::unique_lock<std::mutex> lock(mutex);
    auto result = cond_var.wait_for(lock, std::chrono::seconds(20));
    EXPECT_EQ(result, std::cv_status::no_timeout);
    Sleep(boost::posix_time::millisec(600));
  }
  PrintRoutingTables();
  node->functors_.network_status = nullptr;
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
