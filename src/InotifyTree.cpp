#include "fw/InotifyTree.h"

InotifyTree::InotifyTree(const int inotifyInstance,
                         const fs::path& path,
                         std::shared_ptr<Collector> collector)
  : mCollector(std::move(std::move(collector)))
    , mInotifyInstance(inotifyInstance)
    , mRoot(nullptr) {

  if (!exists(path)) {
    mCollector->sendError("路径不存在");
    return;
  }

  mRoot = new InotifyNode(this, mInotifyInstance, nullptr, path,
                          fs::path(""), false);

  if (!mRoot->isAlive()) {
    mCollector->sendError("意外终止。");
    delete mRoot;
    mRoot = nullptr;
    return;
  }
}

void InotifyTree::sendInitEvent(const fs::path& relPath) const {
  mCollector->collect(CREATED, relPath);
}

InotifyNode::ptr InotifyTree::getInotifyTreeByWatchDescriptor(int watchDescriptor) {
  std::lock_guard locked(mapBlock);

  if (not mInotifyNodeByWatchDescriptor.contains(watchDescriptor)) {
    return nullptr;
  }

  return mInotifyNodeByWatchDescriptor.at(watchDescriptor);
}

void InotifyTree::addDirNode(const int wd,
                             const fs::path& name,
                             const bool sendInitEvents) {
  InotifyNode::ptr const node = getInotifyTreeByWatchDescriptor(wd);

  if (node != nullptr) {
    node->addChild(name, sendInitEvents);
  }
}

void InotifyTree::addNodeReferenceByWD(int wd, InotifyNode::ptr node) {
  std::lock_guard locked(mapBlock);
  mInotifyNodeByWatchDescriptor[wd] = node;
}

bool InotifyTree::getRelPath(fs::path& out, int wd) {
  InotifyNode::ptr node = getInotifyTreeByWatchDescriptor(wd);

  if (node == nullptr) {
    return false;
  }

  out = node->getRelativePath();
  return true;
}

bool InotifyTree::isRootAlive() const { return mRoot != nullptr; }

bool InotifyTree::nodeExists(const int wd) {
  std::lock_guard locked(mapBlock);
  return mInotifyNodeByWatchDescriptor.contains(wd);
}

void InotifyTree::removeDirNode(const int wd, const fs::path& name) {
  InotifyNode::ptr const node = getInotifyTreeByWatchDescriptor(wd);
  if (node != nullptr) {
    node->removeChildNode(name);
  }
}

void InotifyTree::removeDirNode(const int wd) {
  InotifyNode::ptr const node = getInotifyTreeByWatchDescriptor(wd);
  if (node == nullptr) {
    return;
  }

  InotifyNode::ptr const parent = node->getParentNode();
  if (parent == nullptr) {
    mCollector->sendError("意外终止.");
    delete mRoot;
    mRoot = nullptr;
    return;
  }

  parent->removeChildNode(node->getName());
}

void InotifyTree::removeNodeReferenceByWD(int wd) {
  std::lock_guard locked(mapBlock);
  if (mInotifyNodeByWatchDescriptor.contains(wd)) {
    mInotifyNodeByWatchDescriptor.erase(wd);
  }
}

void InotifyTree::moveDirNode(const int wdOld, const fs::path& oldName,
                              const int wdNew, const fs::path& newName) {
  InotifyNode::ptr const node = getInotifyTreeByWatchDescriptor(wdOld);
  if (node == nullptr) {
    return addDirNode(wdNew, newName, true);
  }

  InotifyNode::ptr const movingNode = node->removeAndGetChildNode(oldName);

  if (movingNode == nullptr) {
    return addDirNode(wdNew, newName, true);
  }

  InotifyNode::ptr const nodeNew = getInotifyTreeByWatchDescriptor(wdNew);
  if (nodeNew == nullptr) {
    delete movingNode;
    return;
  }

  movingNode->setNewParentNode(newName, nodeNew);
  nodeNew->insertChildNode(movingNode);
}

void InotifyTree::sendError(const std::string& error) const {
  mCollector->sendError(error);
}

InotifyTree::~InotifyTree() {
  if (isRootAlive()) {
    delete mRoot;
  }
}
