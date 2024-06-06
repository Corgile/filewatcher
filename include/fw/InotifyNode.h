// ReSharper disable CppRedundantQualifier
#ifndef PFW_INOTIFY_NODE_H
#define PFW_INOTIFY_NODE_H

#include <sys/inotify.h>
#include <filesystem>
#include <map>

class InotifyTree;
namespace fs = std::filesystem;

class InotifyNode {
public:
  using ptr = InotifyNode*;
  InotifyNode(InotifyTree* tree,
              int inotifyInstance,
              InotifyNode::ptr parent,
              fs::path fileWatcherRoot,
              fs::path relativePath,
              bool bSendInitEvent);

  void initRecursively(bool bSendInitEvent);
  void addChild(const fs::path& name, bool sendInitEvents);
  void fixPaths();
  fs::path getRelativePath() const;
  fs::path getName() const;
  bool isAlive() const;

  /// remove by name
  void removeChildNode(const fs::path& name);
  InotifyNode::ptr getParentNode() const;
  InotifyNode::ptr removeAndGetChildNode(const fs::path& name);
  void insertChildNode(InotifyNode::ptr childNode);
  void setNewParentNode(const fs::path& filename, InotifyNode::ptr parentNode);

  ~InotifyNode();

private:
  static fs::path
  createFullPath(const fs::path& root, const fs::path& relPath);
  //@format:off
  static constexpr int ATTRIBUTES = IN_ATTRIB | IN_CREATE | IN_DELETE |
                                    IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO |
                                    IN_DELETE_SELF;
  int                                  mWatchDescriptor;
  bool                                 mAlive;
  bool                                 mWatchDescriptorInitialized;
  fs::path                             mRelativePath;
  const int                            mInotifyInstance;
  InotifyTree*                         mTree;
  const fs::path                       mFileWatcherRoot;
  InotifyNode::ptr                     mParent;
  std::map<fs::path, InotifyNode::ptr> mChildren;
  //@format:on
};

#endif
