#ifndef COPYWORKER_H
#define COPYWORKER_H

#include <QDir>
#include <QObject>
#include <QStringList>

class CopyWorker : public QObject
{
    Q_OBJECT
public:
    CopyWorker(const QString &srcPath, const QString &stagePath);
    ~CopyWorker();

public slots:
    void process();

signals:
    void finished();
    void copied(QString filename);

private:
    QDir srcDir;
    QDir stageDir;

    bool copyFolder(const QString &path);
    bool fileIsJump(const QString &fileName);
};

#endif // COPYWORKER_H
