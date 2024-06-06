#ifndef PFW_INOTIFY_TREE_H
#define PFW_INOTIFY_TREE_H

#include <map>
#include <mutex>
#include <filesystem>

#include "fw/Collector.h"
#include "fw/InotifyNode.h"

namespace fs = std::filesystem;

class InotifyTree {
public:
  using ptr = InotifyTree*;
  InotifyTree(int inotifyInstance,
              const fs::path& path,
              Collector::sptr collector);

  bool getRelPath(fs::path& out, int wd);
  bool isRootAlive() const;
  bool nodeExists(int wd);
  void sendInitEvent(const fs::path& relPath) const;

  void addDirNode(int wd, const fs::path& name, bool sendInitEvents);
  void removeDirNode(int wd); // by wd
  void removeDirNode(int wd, const fs::path& name); // by name
  void moveDirNode(int wdOld, const fs::path& oldName, int wdNew, const fs::path& newName);

  ~InotifyTree();

private:
  void sendError(const std::string& error) const;
  void addNodeReferenceByWD(int watchDescriptor, InotifyNode::ptr node);
  void removeNodeReferenceByWD(int watchDescriptor);
  InotifyNode::ptr getInotifyTreeByWatchDescriptor(int watchDescriptor);

  std::mutex mapBlock;
  Collector::sptr mCollector;
  const int mInotifyInstance;
  InotifyNode::ptr mRoot;
  std::map<int, InotifyNode::ptr> mInotifyNodeByWatchDescriptor;
  friend class InotifyNode;
};

#endif
