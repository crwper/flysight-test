#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <windows.h>

#include <QMainWindow>
#include <QString>

class QTableWidgetItem;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    bool nativeEvent(const QByteArray &eventType,
                     void *message, long *result);

private:
    Ui::MainWindow *ui;

    int  copyThreads;
    UINT queryCancelAutoPlay;

    void setRootFolder(QString rootFolder);

    bool fileIsJump(const QString &fileName);

    QString readConfigId(QString fileName);
    bool writeConfigId(QString fileName, QString id);

    void handleDeviceInsert(int driveNum);
    void handleDeviceRemove(int driveNum);

private slots:
    void on_browseButton_clicked();
    void on_removeButton_clicked();
    void on_testButton_clicked();

    void on_fileList_itemSelectionChanged();

    void onCopyFinished();
};

#endif // MAINWINDOW_H
