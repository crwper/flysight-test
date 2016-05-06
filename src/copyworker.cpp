#include <QDir>
#include <QRegExp>

#include "copyworker.h"

CopyWorker::CopyWorker(
        const QString &srcPath,
        const QString &stagePath):
    srcDir(srcPath),
    stageDir(stagePath)
{
    // Initialize here
}

CopyWorker::~CopyWorker()
{
    // Free resources
}

void CopyWorker::process()
{
    if (!stageDir.exists())
    {
        // Create folder
        stageDir.mkpath(".");
    }

    // Read from oldest to newest
    srcDir.setSorting(QDir::Name | QDir::Reversed);

    // Handle child folders
    foreach (QString path, srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        // Check if the folder fits our "date" format
        QRegExp rx("^[0-9]{2}-[0-9]{2}-[0-9]{2}$");

        if (!rx.indexIn(path))
        {
            if (copyFolder(path)) break;
        }
    }

    // Let main thread know we're done
    emit finished();
}

bool CopyWorker::copyFolder(
        const QString &path)
{
    QDir subDir = srcDir.absoluteFilePath(path);

    // Read from oldest to newest
    subDir.setSorting(QDir::Name | QDir::Reversed);

    foreach (QString file, subDir.entryList(QDir::Files))
    {
        // Check if file fits our "time" format
        QRegExp rx("^[0-9]{2}-[0-9]{2}-[0-9]{2}\\.CSV$");

        // Archived/staged filename
        QString newFile = path + "-" + file;

        if (!rx.indexIn(file))
        {
            // Source filename
            QString srcFilePath = subDir.absoluteFilePath(file);

            if (fileIsJump(srcFilePath))
            {
                // Staged filename
                QString stageFilePath = stageDir.absoluteFilePath(newFile);

                // Copy file to "staged" sub-tree
                QFile::copy(srcFilePath, stageFilePath);

                // Let main thread know the file was copied
                emit copied(stageFilePath);

                // Don't stage again
                return true;
            }
        }
    }

    return false;
}

bool CopyWorker::fileIsJump(
        const QString &fileName)
{
    QFileInfo fi(fileName);
    return fi.size() > 1000000;
}
