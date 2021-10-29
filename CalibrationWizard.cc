/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
// design taken from 'examples/dialogs/licensewizard'

#include "CalibrationWizard.h"
#include "TestA2DPage.h"
#include "Calibrator.h"
#include "PolyEval.h"

#include <QApplication>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCursor>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QProgressDialog>
#include <QRadioButton>
#include <QTreeView>
#include <QSocketNotifier>

#include <unistd.h>


CalibrationWizard *CalibrationWizard::_instance;

CalibrationWizard::CalibrationWizard(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : QWizard(parent, Qt::Window), acc(acc), calibrator(calib)
{
    setOption(QWizard::NoBackButtonOnStartPage, true);
    setOption(QWizard::NoBackButtonOnLastPage,  true);
    setOption(QWizard::IndependentPages,        true);
    setOption(QWizard::NoCancelButton,          true);

    _SetupPage   = new SetupPage(calib);
    _TestA2DPage = new TestA2DPage(calib, acc);
    _AutoCalPage = new AutoCalPage(calib, acc);

    setPage(Page_Setup,   _SetupPage);
    setPage(Page_TestA2D, _TestA2DPage);
    setPage(Page_AutoCal, _AutoCalPage);

    setStartId(Page_Setup);

#ifndef Q_WS_MAC
    setWizardStyle(ModernStyle);
#endif
// TODO
//  setPixmap(QWizard::LogoPixmap, QPixmap(":/images/logo.png"));

    setWindowTitle(tr("Auto Calibration Wizard"));

    // setup UNIX signal handler
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGINT);
    sigaddset(&sigset,SIGTERM);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);

    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = CalibrationWizard::sigAction;
    sigaction(SIGHUP ,&act,(struct sigaction *)0);
    sigaction(SIGINT ,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);

    // setup sockets to receive UNIX signals
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, signalFd))
        qFatal("Couldn't create socketpair");

    _snSignal = new QSocketNotifier(signalFd[1], QSocketNotifier::Read, this);
    connect(_snSignal, SIGNAL(activated(int)), this, SLOT(handleSignal()));
    _instance = this;
}

CalibrationWizard::~CalibrationWizard()
{
    delete _snSignal;
    delete _AutoCalPage;
    delete _TestA2DPage;
    delete _SetupPage;
}


// signal handler cleanup work.
void CalibrationWizard::cleanup(int sig, siginfo_t* siginfo, void* vptr)
{
    cout <<
        "received signal " << strsignal(sig) << '(' << sig << ')' <<
        ", si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
        ", si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
        ", si_code=" << (siginfo ? siginfo->si_code : -1) << endl;


    // Clear any residual auto-cal
    acc->SetNextCalVoltage(DONE);

    char a = 1;

    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        ::write(signalFd[0], &a, sizeof(a));
        break;
    }
}


void CalibrationWizard::handleSignal()
{
    char tmp;
    ::read(signalFd[1], &tmp, sizeof(tmp));

    // do Qt stuff
    emit close();
}


/* static */
int CalibrationWizard::signalFd[2]  = {0, 0};


void CalibrationWizard::accept()
{
    calibrator->cancel();
    calibrator->wait();
    QWizard::accept();
}


void CalibrationWizard::closeEvent(QCloseEvent *event)
{
    cout << __PRETTY_FUNCTION__ << endl;
    if (!calibrator) return;

    calibrator->cancel();
    calibrator->wait();
    exit(0);
}


/* ---------------------------------------------------------------------------------------------- */

SetupPage::SetupPage(Calibrator *calib, QWidget *parent)
    : QWizardPage(parent), calibrator(calib)
{
    setTitle(tr("Setup"));

    // TODO
//  setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/watermark.png"));

    topLabel = new QLabel(tr("This will search the NIDAS server and all of its "
                             "connected DSMs for NCAR based A2D cards.\n\nAll "
                             "cards can only operate as configured.\n\nYou can "
                             "either test a card's channels by manually setting "
                             "them, or you can automatically calibrate all of "
                             "the cards:\n"));
    topLabel->setWordWrap(true);

    testa2dRadioButton = new QRadioButton(tr("&Test A2D channels"));
    autocalRadioButton = new QRadioButton(tr("&Auto Calibrate"));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(topLabel);
    layout->addWidget(testa2dRadioButton);
    layout->addWidget(autocalRadioButton);
    setLayout(layout);
}


int SetupPage::nextId() const
{
    if (autocalRadioButton->isChecked()) {
        return CalibrationWizard::Page_AutoCal;
    }
    else if (testa2dRadioButton->isChecked()) {
        return CalibrationWizard::Page_TestA2D;
    }
    return CalibrationWizard::Page_Setup;
}

/* ---------------------------------------------------------------------------------------------- */

AutoCalPage::AutoCalPage(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : QWizardPage(parent), dsmId(-1), devId(-1), calibrator(calib), acc(acc)
{
    setTitle(tr("Auto Calibration"));
    setSubTitle(tr("Select a card from the tree to review the results."));
    setFinalPage(true);
}


void AutoCalPage::setVisible(bool visible)
{
    QWizardPage::setVisible(visible);

    if (visible) {
        wizard()->setButtonText(QWizard::CustomButton1, tr("&Save"));
        wizard()->setOption(QWizard::HaveCustomButton1, true);
        connect(wizard(), SIGNAL(customButtonClicked(int)),
                this, SLOT(saveButtonClicked()));
    } else {
        wizard()->setOption(QWizard::HaveCustomButton1, false);
        disconnect(wizard(), SIGNAL(customButtonClicked(int)),
                   this, SLOT(saveButtonClicked()));
    }
}


void AutoCalPage::saveButtonClicked()
{
/* original behavior was to save the card selected on the GUI.
    if (dsmId == devId) {
        QMessageBox::information(0, "notice", "You must select a card to save!");
        return;
    }

    acc->SaveCalFile(dsmId, devId);
*/

    // New default behavior is to save all cards in one fell swoop.
    acc->SaveAllCalFiles();
}


void AutoCalPage::createTree()
{
    cout << "AutoCalPage::createTree" << endl;
    treeView = new QTreeView();

    // extract the tree from AutoCalClient.
    cout << acc->GetTreeModel();
    treeModel = new TreeModel( QString(acc->GetTreeModel().c_str()) );

    // Initialize the QTreeView
    treeView->setModel(treeModel);
    treeView->expandAll();
    treeView->setMinimumWidth(300);
    treeView->resizeColumnToContents(0);
    treeView->resizeColumnToContents(1);
    treeView->resizeColumnToContents(2);

    // The dsmId(s) and devId(s) are hidden in the 3rd column.
//  treeView->hideColumn(2);

    connect(treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
                                    this, SLOT(selectionChanged(const QItemSelection&, const QItemSelection&)));

    treeView->setCurrentIndex(treeView->model()->index(0,0));
}


void AutoCalPage::selectionChanged(const QItemSelection &selected, const QItemSelection &/*deselected*/)
{
    if (selected.indexes().count() == 0)
        return;

    QModelIndex index = selected.indexes().first();
    QModelIndex parent = index.parent();

    if (parent == QModelIndex()) {
        dsmId = devId;
        return;
    }
    QModelIndex devIdx = index.sibling(index.row(), 2);
    devId = treeModel->data(devIdx, Qt::DisplayRole).toInt();

    QModelIndex dsmIdx = parent.sibling(parent.row(), 2);
    dsmId = treeModel->data(dsmIdx, Qt::DisplayRole).toInt();

    for (int chn = 0; chn < numA2DChannels; chn++) {
        VarName[chn]->setText( QString( acc->GetVarName(dsmId, devId, chn).c_str() ) );

        OldTimeStamp[chn]->setText( QString( acc->GetOldTimeStamp(dsmId, devId, chn).c_str() ) );
        NewTimeStamp[chn]->setText( QString( acc->GetNewTimeStamp(dsmId, devId, chn).c_str() ) );

        OldTemperature[chn]->setText( QString::number( acc->GetOldTemperature(dsmId, devId, chn) ) );
        NewTemperature[chn]->setText( QString::number( acc->GetNewTemperature(dsmId, devId, chn) ) );

        OldIntcp[chn]->setText( QString::number( acc->GetOldCals(dsmId, devId, chn)[0] ) );
        NewIntcp[chn]->setText( QString::number( acc->GetNewCals(dsmId, devId, chn)[1] ) );

        OldSlope[chn]->setText( QString::number( acc->GetOldCals(dsmId, devId, chn)[0] ) );
        NewSlope[chn]->setText( QString::number( acc->GetNewCals(dsmId, devId, chn)[1] ) );
    }
}


void AutoCalPage::createGrid()
{
    gridGroupBox = new QGroupBox(tr("Auto Cal Results"));

    QGridLayout *layout = new QGridLayout;

    ChannelTitle     = new QLabel( QString( "CHN" ) );
    VarNameTitle     = new QLabel( QString( "VARNAME" ) );
    TimeStampTitle   = new QLabel( QString( "TIME" ) );
    IntcpTitle       = new QLabel( QString( "INTCP" ) );
    SlopeTitle       = new QLabel( QString( "SLOPE" ) );

    TemperatureTitle = new QLabel( QString( "TEMP" ) );

    QFont font;
    #if defined(Q_WS_X11)
    font.setFamily("Monospace");
    #else
    font.setFamily("Courier New");
    #endif
    font.setPointSize(9);
    setFont(font);

    layout->addWidget( ChannelTitle,   0, 0);
    layout->addWidget( VarNameTitle,   0, 1);
    layout->addWidget( TimeStampTitle, 0, 2);
    layout->addWidget( IntcpTitle,     0, 3);
    layout->addWidget( SlopeTitle,     0, 4);

    layout->setColumnMinimumWidth( 2, 174);

//  layout->addWidget( TemperatureTitle, 0, 2);

    for (int chn = 0; chn < numA2DChannels; chn++) {
        Channel[chn] = new QLabel( QString("%1:").arg(chn) );
        layout->addWidget(Channel[chn], chn*3+2, 0, 2, 1);

        VarName[chn] = new QLabel;
        layout->addWidget(VarName[chn], chn*3+2, 1, 2, 1);

//      layout->setRowMinimumHeight(chn*2, 30);

        OldTimeStamp[chn]   = new QLabel;
        OldTemperature[chn] = new QLabel;
        OldIntcp[chn]       = new QLabel;
        OldSlope[chn]       = new QLabel;

        NewTimeStamp[chn]   = new QLabel;
        NewTemperature[chn] = new QLabel;
        NewIntcp[chn]       = new QLabel;
        NewSlope[chn]       = new QLabel;

        // add a "blank line" between channels
        layout->addWidget(new QLabel, (chn*3)+1, 0, 1, 5);

        layout->addWidget(OldTimeStamp[chn],   (chn*3)+2, 2);
        layout->addWidget(OldIntcp[chn],       (chn*3)+2, 3);
        layout->addWidget(OldSlope[chn],       (chn*3)+2, 4);

        layout->addWidget(NewTimeStamp[chn],   (chn*3)+3, 2);
        layout->addWidget(NewIntcp[chn],       (chn*3)+3, 3);
        layout->addWidget(NewSlope[chn],       (chn*3)+3, 4);
    }
//  layout->addWidget(OldTemperature[chn], (chn*3)+2, 2);
//  layout->addWidget(NewTemperature[chn], (chn*3)+3, 2);

//  gridGroupBox->setMinimumSize(450, 600); // width, height
    gridGroupBox->setLayout(layout);
}


void AutoCalPage::initializePage()
{
    cout << "AutoCalPage::initializePage" << endl;

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    if (calibrator->setup("acserver")) return;
    QApplication::restoreOverrideCursor();

    createTree();
    createGrid();

    mainLayout = new QHBoxLayout;
    mainLayout->addWidget(treeView);
    mainLayout->addWidget(gridGroupBox);

    setLayout(mainLayout);

    qPD = new QProgressDialog(this);
    qPD->setRange(0, acc->maxProgress() );
    qPD->setWindowTitle(tr("Auto Calibrating..."));
    qPD->setWindowModality(Qt::WindowModal);

    // This connection spans across threads so it is a
    // Qt::QueuedConnection by default.
    // (http://doc.qt.nokia.com/4.6/threads-mandelbrot.html)
    connect(acc,  SIGNAL(errMessage(const QString&)),
            this,   SLOT(errMessage(const QString&)));

    connect(calibrator, SIGNAL(setValue(int)),
            qPD,          SLOT(setValue(int)) );

    connect(qPD,        SIGNAL(canceled()),
            calibrator,   SLOT(cancel()) );

    calibrator->start();  // see Calibrator::run
}


void AutoCalPage::errMessage(const QString& message)
{
    QMessageBox::warning(this, "error", message);
}


void AutoCalPage::setValue(int progress)
{
    qPD->setValue(progress);
};
