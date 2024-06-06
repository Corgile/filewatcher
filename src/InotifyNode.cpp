// ReSharper disable CppRedundantQualifier
#include <ranges>
#include "fw/InotifyNode.h"
#include "fw/InotifyTree.h"

InotifyNode::InotifyNode(const InotifyTree::ptr tree,
                         const int inotifyInstance,
                         const InotifyNode::ptr parent,
                         fs::path fileWatcherRoot,
                         fs::path relativePath,
                         const bool bSendInitEvent)
  : mWatchDescriptorInitialized(false)
    , mRelativePath(std::move(relativePath))
    , mInotifyInstance(inotifyInstance)
    , mTree(tree)
    , mFileWatcherRoot(std::move(fileWatcherRoot))
    , mParent(parent) {
  const int event_mask = mParent != nullptr ? ATTRIBUTES : ATTRIBUTES | IN_MOVE_SELF;

  mWatchDescriptor = inotify_add_watch(
    mInotifyInstance, (mFileWatcherRoot / mRelativePath).c_str(), event_mask);

  mAlive = mWatchDescriptor != -1;

  if (!mAlive) {
    if (errno == EACCES) {
      mTree->sendError("无权限： " + mRelativePath.string());
    } else if (errno == EFAULT) {
      mTree->sendError("bad adress");
    } else if (errno == ENOSPC) {
      mTree->sendError("No space left on device");
    } else if (errno == ENOMEM) {
      mTree->sendError("no mem");
    } else if (errno == EBADF || errno == EINVAL) {
      mTree->sendError("bad file number / invalid arg");
    }

    return;
  }

  std::error_code ec;
  auto status = fs::status(mFileWatcherRoot / mRelativePath, ec);
  if (ec || !is_directory(status) || is_symlink(status)) {
    inotify_rm_watch(mInotifyInstance, mWatchDescriptor);
    mAlive = false;
    return;
  }

  mWatchDescriptorInitialized = true;
  mTree->addNodeReferenceByWD(mWatchDescriptor, this);

  initRecursively(bSendInitEvent);
}

auto InotifyNode::initRecursively(const bool bSendInitEvent) -> void {
  std::error_code ec;
  auto dirItr = fs::directory_iterator(mFileWatcherRoot / mRelativePath, ec);
  if (ec) { return; }
  for (auto& child : dirItr) {
    std::error_code statusEc;
    auto status = fs::status(child);
    if (statusEc || is_symlink(status)) { continue; }

    const auto filename = child.path().filename();

    if (is_directory(status)) {
      auto* childInotifyNode =
        new InotifyNode(mTree, mInotifyInstance,
                        this, mFileWatcherRoot,
                        mRelativePath / filename, bSendInitEvent);

      if (childInotifyNode->isAlive()) {
        mChildren[filename] = childInotifyNode;
      } else {
        delete childInotifyNode;
      }
    }

    if (bSendInitEvent) {
      mTree->sendInitEvent(mRelativePath / filename);
    }
  }
}

InotifyNode::~InotifyNode() {
  if (mWatchDescriptorInitialized) {
    inotify_rm_watch(mInotifyInstance, mWatchDescriptor);
    mTree->removeNodeReferenceByWD(mWatchDescriptor);
  }

  for (const auto inotifyNode : mChildren | std::ranges::views::values) {
    delete inotifyNode;
  }
  // delete mChildren;
}

void InotifyNode::addChild(const fs::path& name,
                           const bool sendInitEvents) {
  auto* child =
    new InotifyNode(mTree, mInotifyInstance, this, mFileWatcherRoot,
                    mRelativePath / name, sendInitEvents);

  if (child->isAlive()) {
    mChildren[name] = child;
  } else {
    delete child;
  }
}

void InotifyNode::fixPaths() {
  const auto relPath = mParent->getRelativePath() / mRelativePath.filename();
  if (relPath == mRelativePath) return;
  mRelativePath = relPath;
  for (const auto inotifyNode : mChildren | std::ranges::views::values) {
    inotifyNode->fixPaths();
  }
}

fs::path InotifyNode::getRelativePath() const { return mRelativePath; }

fs::path InotifyNode::getName() const { return mRelativePath.filename(); }

bool InotifyNode::isAlive() const { return mAlive; }

InotifyNode::ptr InotifyNode::getParentNode() const { return mParent; }

void InotifyNode::removeChildNode(const fs::path& name) {
  if (mChildren.contains(name)) {
    delete mChildren.at(name);
    mChildren.at(name) = nullptr;
    mChildren.erase(name);
  }
}

InotifyNode::ptr InotifyNode::removeAndGetChildNode(const fs::path& name) {
  InotifyNode::ptr result = nullptr;
  if (mChildren.contains(name)) {
    result = std::move(mChildren.at(name));
    mChildren.erase(name);
  }
  return result;
}

void InotifyNode::insertChildNode(const InotifyNode::ptr childNode) {
  mChildren[childNode->getName()] = childNode;
}

void InotifyNode::setNewParentNode(const fs::path& filename,
                               const InotifyNode::ptr parentNode) {
  if (mRelativePath == fs::path() || parentNode == nullptr) {
    return;
  }
  mRelativePath = mRelativePath.parent_path() / filename;
  mParent = parentNode;
  fixPaths();
}

fs::path
InotifyNode::createFullPath(const fs::path& root,
                            const fs::path& relPath) {
  return root / relPath;
}
