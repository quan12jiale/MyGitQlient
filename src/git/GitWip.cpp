#include "GitWip.h"

#include <GitBase.h>
#include <GitCache.h>

#include <QLogger.h>

#include <QFile>

using namespace QLogger;

GitWip::GitWip(const QSharedPointer<GitBase> &git, const QSharedPointer<GitCache> &cache)
   : mGit(git)
   , mCache(cache)
{
}

QVector<QString> GitWip::getUntrackedFiles() const
{
   QLog_Debug("Git", QString("Executing getUntrackedFiles."));

   auto runCmd = QString("git ls-files --others --exclude-standard");

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
   const auto ret = mGit->run(runCmd).output.split('\n', Qt::SkipEmptyParts).toVector();
#else
   const auto ret = mGit->run(runCmd).output.split('\n', QString::SkipEmptyParts).toVector();
#endif

   return ret;
}

std::pair<QPair<QString, RevisionFiles>, bool> GitWip::getWipInfo() const
{
   QLog_Debug("Git", QString("Executing processWip."));

   const auto ret = mGit->run("git rev-parse --revs-only HEAD");

   if (ret.success)
   {
      QString diffIndex;
      QString diffIndexCached;

      auto parentSha = ret.output.trimmed();

      if (parentSha.isEmpty())
         parentSha = CommitInfo::INIT_SHA;

      const auto ret3 = mGit->run(QString("git diff-index %1").arg(parentSha));
      diffIndex = ret3.success ? ret3.output : QString();

      const auto ret4 = mGit->run(QString("git diff-index --cached %1").arg(parentSha));
      diffIndexCached = ret4.success ? ret4.output : QString();

      auto files = fakeWorkDirRevFile(diffIndex, diffIndexCached);

      return std::make_pair(qMakePair(parentSha, std::move(files)), true);
   }

   return std::make_pair(qMakePair(QString{}, RevisionFiles{}), false);
}

std::pair<GitWip::FileStatus, bool> GitWip::getFileStatus(const QString &filePath) const
{
   QLog_Debug("Git", QString("Getting file status."));

   const auto ret = mGit->run(QString("git diff-files -c %1").arg(filePath));

   if (ret.success)
   {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
      const auto lines = ret.output.split("\n", Qt::SkipEmptyParts);
#else
      const auto lines = ret.output.split("\n", QString::SkipEmptyParts);
#endif

      if (lines.count() > 1)
         return std::make_pair(FileStatus::DeletedByThem, true);
      else
      {
         const auto statusField = lines[0].split(" ").last().split("\t").constFirst();

         if (statusField.count() == 2)
            return std::make_pair(FileStatus::BothModified, true);

         return std::make_pair(FileStatus::DeletedByUs, true);
      }
   }

   return std::make_pair(FileStatus::BothModified, false);
}

bool GitWip::updateWip() const
{
   const auto files = getUntrackedFiles();
   mCache->setUntrackedFilesList(std::move(files));

   const auto info = getWipInfo();
   if (info.second)
   {
      return mCache->updateWipCommit(info.first.first, info.first.second);
   }

   return false;
}

RevisionFiles GitWip::fakeWorkDirRevFile(const QString &diffIndex, const QString &diffIndexCache) const
{
   RevisionFiles rf(diffIndex);
   rf.setOnlyModified(false);

   for (const auto &it : mCache->getUntrackedFiles())
   {
      rf.mFiles.append(it);
      rf.setStatus(RevisionFiles::UNKNOWN);
      rf.mergeParent.append(1);
   }

   RevisionFiles cachedFiles(diffIndexCache, true);

   for (auto i = 0; i < rf.count(); i++)
   {
	   const auto cachedIndex = cachedFiles.mFiles.indexOf(rf.getFile(i));
      if (cachedIndex != -1)
      {
         if (cachedFiles.statusCmp(cachedIndex, RevisionFiles::CONFLICT))
         {
            rf.appendStatus(i, RevisionFiles::CONFLICT);

            const auto status = getFileStatus(rf.getFile(i));

            switch (status.first)
            {
               case GitWip::FileStatus::DeletedByThem:
               case GitWip::FileStatus::DeletedByUs:
                  rf.appendStatus(i, RevisionFiles::DELETED);
                  break;
               default:
                  break;
            }
         }
         else if (cachedFiles.statusCmp(cachedIndex, RevisionFiles::MODIFIED)
                  && cachedFiles.statusCmp(cachedIndex, RevisionFiles::IN_INDEX))
            rf.appendStatus(i, RevisionFiles::PARTIALLY_CACHED);
         else if (cachedFiles.statusCmp(cachedIndex, RevisionFiles::IN_INDEX))
            rf.appendStatus(i, RevisionFiles::IN_INDEX);
      }
   }

   return rf;
}
