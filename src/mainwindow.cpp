#include <math.h>
#include <windows.h>
#include <dbt.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include "configdialog.h"
#include "copyworker.h"
#include "mainwindow.h"

#include "ui_mainwindow.h"

#define EXPORT_BUTTON_DELAY 1000 // ms

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    copyThreads(0)
{
    // Initialize UI object
    ui->setupUi(this);

    // Message for AutoRun notifications
    queryCancelAutoPlay = RegisterWindowMessage(L"QueryCancelAutoPlay");

    // Initialize settings object
    QSettings settings("FlySight", "Test");

    // Get root folder
    QString rootFolder = settings.value("rootFolder").toString();

    if (!rootFolder.isEmpty())
    {
        // Update destination folder
        setRootFolder(rootFolder);
    }
    else
    {
        // Use temporary folder
        setRootFolder(QDir::tempPath());
    }

    // Initialize button states
    on_fileList_itemSelectionChanged();

    // File list double-click
    connect(ui->fileList,   SIGNAL(itemDoubleClicked(QTableWidgetItem*)),
            this,           SLOT(exportItem(QTableWidgetItem*)));
}

MainWindow::~MainWindow()
{
    // Initialize settings object
    QSettings settings("FlySight", "Test");

    // Write INI filename
    settings.setValue("rootFolder", ui->dstEdit->text());

    // Destroy UI object
    delete ui;
}

void MainWindow::setRootFolder(
        QString rootFolder)
{
    QDir rootDir(rootFolder);

    // Update dialog box
    ui->dstEdit->setText(rootFolder);

    // Update file lists
    ui->fileList->setRootFolder(rootDir.absoluteFilePath("staged"));
}

void MainWindow::on_browseButton_clicked()
{
    // Get new settings file
    QString rootFolder = QFileDialog::getExistingDirectory(
                this,
                "",
                ui->dstEdit->text());

    if (!rootFolder.isEmpty())
    {
        // Update destination folder
        setRootFolder(rootFolder);
    }
}

void MainWindow::on_removeButton_clicked()
{
    if (QMessageBox::Ok == QMessageBox::warning(this,
                                                "Remove Jumps",
                                                "Remove the selected jumps from the list?",
                                                QMessageBox::Ok | QMessageBox::Cancel,
                                                QMessageBox::Cancel))
    {
        QDir rootDir(ui->dstEdit->text());
        QDir stagedDir(rootDir.absoluteFilePath("staged"));

        foreach (QTableWidgetItem *item,
                 ui->fileList->selectedItems())
        {
            // Delete file from folder
            QFile::remove(stagedDir.absoluteFilePath(item->text()));
        }
    }
}

void MainWindow::on_testButton_clicked()
{
    QDir rootDir(ui->dstEdit->text());
    QDir stagedDir(rootDir.absoluteFilePath("staged"));

    foreach (QTableWidgetItem *item,
             ui->fileList->selectedItems())
    {
        QFile file(stagedDir.absoluteFilePath(item->text()));
        if (!file.open(QIODevice::ReadOnly)) return;

        QTextStream in(&file);

        // Column enumeration
        typedef enum {
            Time = 0,
            Lat,
            Lon,
            HMSL,
            VelN,
            VelE,
            VelD,
            HAcc,
            VAcc,
            SAcc,
            Heading,
            CAcc,
            NumSV,
            NumColumns
        } Columns;

        // Read column labels
        QMap< int, int > colMap;
        if (!in.atEnd())
        {
            QString line = in.readLine();
            QStringList cols = line.split(",");

            for (int i = 0; i < cols.size(); ++i)
            {
                const QString &s = cols[i];

                if (s == "time")    colMap[Time]    = i;
                if (s == "lat")     colMap[Lat]     = i;
                if (s == "lon")     colMap[Lon]     = i;
                if (s == "hMSL")    colMap[HMSL]    = i;
                if (s == "velN")    colMap[VelN]    = i;
                if (s == "velE")    colMap[VelE]    = i;
                if (s == "velD")    colMap[VelD]    = i;
                if (s == "hAcc")    colMap[HAcc]    = i;
                if (s == "vAcc")    colMap[VAcc]    = i;
                if (s == "sAcc")    colMap[SAcc]    = i;
                if (s == "numSV")   colMap[NumSV]   = i;
            }
        }

        // Skip next row
        if (!in.atEnd()) in.readLine();

        // Initialize statistics
        bool first = true;

        QDateTime startTime, endTime;
        double s1 = 0, s2 = 0;
        int n = 0;

        while (!in.atEnd())
        {
            QString line = in.readLine();
            QStringList cols = line.split(",");

            if (cols.length() < NumColumns) continue;

            QDateTime dateTime = QDateTime::fromString(cols[colMap[Time]], Qt::ISODate);
            int numSV = cols[colMap[NumSV]].toInt();

            if (first)
            {
                startTime = dateTime;
                first = false;
            }
            else
            {
                endTime = dateTime;
            }

            s1 += numSV;
            s2 += numSV * numSV;

            ++n;
        }

        // Calculate length of log file
        qint64 start = startTime.toMSecsSinceEpoch();
        qint64 end = endTime.toMSecsSinceEpoch();
        double length = (double) (end - start) / 1000 / 60 / 60;

        int row = item->row();
        QTableWidgetItem *newItem = new QTableWidgetItem(tr("%1").arg(length));
        ui->fileList->setItem(row, 1, newItem);

        newItem = new QTableWidgetItem(tr("%1").arg(s1 / n));
        ui->fileList->setItem(row, 2, newItem);

        newItem = new QTableWidgetItem(tr("%1").arg(sqrt(n * s2 - s1 * s1) / n));
        ui->fileList->setItem(row, 3, newItem);
    }
}

void MainWindow::on_fileList_itemSelectionChanged()
{
    // Enable buttons
    ui->removeButton->setEnabled(ui->fileList->selectedItems().size() > 0);
    ui->testButton->setEnabled(ui->fileList->selectedItems().size() > 0);
}

void MainWindow::onCopyFinished()
{
    // Decrement thread counter
    if (--copyThreads == 0)
    {
        ui->statusBar->clearMessage();
    }
}

bool MainWindow::nativeEvent(
        const QByteArray &, // unused
        void             *message,
        long             *result)
{
    MSG *msg = (MSG *) message;

    if (msg->message == WM_DEVICECHANGE)
    {
        bool handle = false;

        switch (msg->wParam)
        {
        case DBT_DEVICEARRIVAL:
            handle = true;
            break;
        case DBT_DEVICEREMOVECOMPLETE:
            handle = true;
            break;
        }

        if (handle && msg->lParam)
        {
            DEV_BROADCAST_HDR *pHdr = (DEV_BROADCAST_HDR *) msg->lParam;

            if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME)
            {
                DEV_BROADCAST_VOLUME *pVol = (DEV_BROADCAST_VOLUME *) pHdr;

                for (int driveNum = 0, mask = pVol->dbcv_unitmask;
                     mask;
                     ++driveNum, mask >>= 1)
                {
                    if (mask & 1)
                    {
                        switch (msg->wParam)
                        {
                        case DBT_DEVICEARRIVAL:
                            handleDeviceInsert(driveNum);
                            break;
                        case DBT_DEVICEREMOVECOMPLETE:
                            handleDeviceRemove(driveNum);
                            break;
                        }
                    }
                }
            }
        }
    }

    if (msg->message == queryCancelAutoPlay)
    {
        // Disable AutoRun
        *(result) = 1;
        return true;
    }

    return false;
}

QString MainWindow::readConfigId(
        QString fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();

    QTextStream in(&file);

    QString id;

    while (!in.atEnd())
    {
        QString line = in.readLine();

        // Get rid of comments
        line = line.split(";")[0];

        // Parse the rest
        QStringList cols = line.split(":");
        if (cols.length() < 2) continue;

        QString name  = cols[0].trimmed();
        QString value = cols[1].trimmed();

        // Look for ID field
        if (!name.compare("id", Qt::CaseInsensitive))
        {
            id = value;
            break;
        }
    }

    return id;
}

bool MainWindow::writeConfigId(
        QString fileName,
        QString id)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);

    out << "; GPS settings" << endl;
    out << endl;
    out << "Model:     7    ; Airborne with < 2 G acceleration" << endl;
    out << "Rate:      200  ; Measurement rate (ms)" << endl;
    out << endl;
    out << "; Competition settings" << endl;
    out << "Id:        " << id << endl;

    return true;
}

void MainWindow::handleDeviceInsert(
        int driveNum)
{
    QString rootPath = QString("%1:\\")
            .arg(QString("ABCDEFGHIJKLMNOPQRSTUVWXYZ").at(driveNum));
    QString configPath(rootPath + QString("CONFIG.TXT"));


    // Run the configuration dialog
    ConfigDialog dlg(this);
    if (!dlg.exec()) return;

    // Get unit name
    QString id = dlg.name().trimmed();

    // Create worker thread
    QThread *thread = new QThread;

    // Create worker thread controller
    CopyWorker *worker = new CopyWorker(
                rootPath,
                ui->dstEdit->text() + QDir::separator() + "staged" + QDir::separator() + id);
    worker->moveToThread(thread);

    // Connect worker thread to controller
    connect(thread, SIGNAL(started()),  worker, SLOT(process()));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(worker, SIGNAL(finished()), this, SLOT(onCopyFinished()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    // Increment thread counter
    if (copyThreads++ == 0)
    {
        ui->statusBar->showMessage("Copying files...");
    }

    // Start worker thread
    thread->start();
}

void MainWindow::handleDeviceRemove(
        int driveNum)
{
}
