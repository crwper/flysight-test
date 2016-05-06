#include "configdialog.h"
#include "ui_configdialog.h"

ConfigDialog::ConfigDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigDialog)
{
    // Initialize UI object
    ui->setupUi(this);

    // Message for AutoRun notifications
    queryCancelAutoPlay = RegisterWindowMessage(L"QueryCancelAutoPlay");
}

ConfigDialog::~ConfigDialog()
{
    // Destroy UI object
    delete ui;
}

void ConfigDialog::setName(
        const QString &newName)
{
    ui->name->setText(newName);
}

QString ConfigDialog::name() const
{
    return ui->name->text();
}

bool ConfigDialog::nativeEvent(
        const QByteArray &, // unused
        void             *message,
        long             *result)
{
    MSG *msg = (MSG *) message;

    if (msg->message == queryCancelAutoPlay)
    {
        // Disable AutoRun
        *(result) = 1;
        return true;
    }

    return false;
}
