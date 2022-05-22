#pragma once

#include <QSharedPointer>

#include <RevisionFiles.h>
#include <WipRevisionInfo.h>

//#include <optional>

class GitBase;
class GitCache;

class GitWip
{
public:
   enum class FileStatus
   {
      BothModified,
      DeletedByThem,
      DeletedByUs
   };

   explicit GitWip(const QSharedPointer<GitBase> &git, const QSharedPointer<GitCache> &cache);

   QVector<QString> getUntrackedFiles() const;
   bool updateWip() const;
   std::pair<QPair<QString, RevisionFiles>, bool> getWipInfo() const;
   std::pair<FileStatus, bool> getFileStatus(const QString &filePath) const;

private:
   QSharedPointer<GitBase> mGit;
   QSharedPointer<GitCache> mCache;

   RevisionFiles fakeWorkDirRevFile(const QString &diffIndex, const QString &diffIndexCache) const;
};
