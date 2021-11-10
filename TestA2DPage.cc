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

#include "TestA2DPage.h"
#include "PolyEval.h"

#include <unistd.h>

#include <QApplication>
#include <QButtonGroup>
#include <QCursor>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTextStream>
#include <QTreeView>


/* ---------------------------------------------------------------------------------------------- */

TestA2DPage::TestA2DPage(Calibrator *calib, AutoCalClient *acc, QWidget *parent)
    : QWizardPage(parent), dsmId(-1), devId(-1), calibrator(calib), acc(acc)
{
    setTitle(tr("Test A2Ds"));
    setSubTitle(tr("Select a card from the tree to list channels."));
    setFinalPage(true);
    setMinimumWidth(880);
}


TestA2DPage::~TestA2DPage()
{
    cout << "TestA2DPage::~TestA2DPage()" << endl;
    list<int> voltageLevels = acc->GetVoltageLevels();
    list<int>::iterator l;

    for (int chn = 0; chn < numA2DChannels; chn++)
        for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++)
            delete vLvlBtn[*l][chn];
}


void TestA2DPage::createTree()
{
    cout << "TestA2DPage::createTree" << endl;
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
}


void TestA2DPage::dispVolts()
{
    if (devId == -1) return;
    if (dsmId == -1) return;
    if (dsmId == devId) return;

    for (int chn = 0; chn < numA2DChannels; chn++) {
        if ( acc->calActv[0][dsmId][devId][chn] == SKIP ) continue;

        // obtain current set of calibration coefficients for this channel
        std::vector<double> _cals = acc->GetOldCals(dsmId, devId, chn);

        // apply the coefficients to the raw measured values
        QString raw, mes;
        float voltage = acc->testData[dsmId][devId][chn];
        float applied = numeric::PolyEval(_cals, voltage);
        QTextStream rstr(&raw);
        rstr << qSetFieldWidth(7) << qSetRealNumberPrecision(4) << voltage;
        QTextStream mstr(&mes);
        mstr << qSetFieldWidth(7) << qSetRealNumberPrecision(4) << applied;
        RawVolt[chn]->setText( raw );
        MesVolt[chn]->setText( mes );
    }
}


void TestA2DPage::updateSelection()
{
    cout << "TestA2DPage::updateSelection" << endl;
    a2d_setup setup = acc->GetA2dSetup(dsmId, devId);

    for (int chn = 0; chn < numA2DChannels; chn++) {
        VarName[chn]->setText( QString( acc->GetVarName(dsmId, devId, chn).c_str() ) );
        RawVolt[chn]->setText("");
        MesVolt[chn]->setText("");

        VarName[chn]->setHidden(false);
        RawVolt[chn]->setHidden(false);
        MesVolt[chn]->setHidden(false);

        list<int> voltageLevels;
        list<int>::iterator l;

        // hide and uncheck all voltage selection buttons for this channel
        voltageLevels = acc->GetVoltageLevels();
        for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++) {
            vLvlBtn[*l][chn]->setHidden(true);
            vLvlBtn[*l][chn]->setDown(false);
//          cout << "TestA2DPage::updateSelection vLvlBtn[" << *l << "][" << chn << "]->setDown(false);" << endl;
        }
        voltageLevels.clear();

        // show available voltage selection buttons for this channel
        voltageLevels = acc->GetVoltageLevels(dsmId, devId, chn);
        if (!voltageLevels.empty()) {
            for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++)
                vLvlBtn[*l][chn]->setHidden(false);

            RawVolt[chn]->setText("---");
            MesVolt[chn]->setText("---");
            vLvlBtn[-99][chn]->setHidden(false);

            // check all active channels to use the same calibration voltage
            if (setup.calset[chn])
                vLvlBtn[setup.vcal][chn]->setDown(true);
            else
                vLvlBtn[-99][chn]->setDown(true);
        }
    }
}


void TestA2DPage::selectionChanged(const QItemSelection &selected, const QItemSelection &/*deselected*/)
{
    cout << "TestA2DPage::selectionChanged" << endl;
    if (selected.indexes().count() > 0) {

        QModelIndex index = selected.indexes().first();
        QModelIndex parent = index.parent();

        QModelIndex devIdx = index.sibling(index.row(), 2);
        devId = treeModel->data(devIdx, Qt::DisplayRole).toInt();

        QModelIndex dsmIdx = parent.sibling(parent.row(), 2);
        dsmId = treeModel->data(dsmIdx, Qt::DisplayRole).toInt();

        if (parent == QModelIndex()) {

            // DSM selected not card... hide the displayed table contents.
            list<int> voltageLevels;
            list<int>::iterator l;
            voltageLevels = acc->GetVoltageLevels();
            for (int chn = 0; chn < numA2DChannels; chn++) {
                VarName[chn]->setHidden(true);
                RawVolt[chn]->setHidden(true);
                MesVolt[chn]->setHidden(true);
                for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++)
                    vLvlBtn[*l][chn]->setHidden(true);
            }
            dsmId = devId;
            return;
        }
        acc->setTestVoltage(dsmId, devId);
        updateSelection();
    }
}


void TestA2DPage::createGrid()
{
    cout << "TestA2DPage::createGrid" << endl;
    gridGroupBox = new QGroupBox(tr("Set internal voltages here"));

    QGridLayout *layout = new QGridLayout;

    ChannelTitle     = new QLabel( QString( "CHN" ) );
    VarNameTitle     = new QLabel( QString( "VARNAME" ) );
    RawVoltTitle     = new QLabel( QString( "RAWVOLT" ) );
    MesVoltTitle     = new QLabel( QString( "MESVOLT" ) );
    SetVoltTitle     = new QLabel( QString( "SETVOLT" ) );

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
    layout->addWidget( RawVoltTitle,   0, 2);
    layout->addWidget( MesVoltTitle,   0, 3);
    layout->addWidget( SetVoltTitle,   0, 4, 1, 6);

    layout->setColumnMinimumWidth(0, 20);

    list<int> voltageLevels = acc->GetVoltageLevels();

    for (int chn = 0; chn < numA2DChannels; chn++) {
        Channel[chn] = new QLabel( QString("%1:").arg(chn) );
        Channel[chn]->setStyleSheet("QLabel { min-width: 30px ; max-width: 30px }");
        layout->addWidget(Channel[chn], chn+1, 0);

        VarName[chn] = new QLabel;
        VarName[chn]->setStyleSheet("QLabel { min-width: 100px }");
        layout->addWidget(VarName[chn], chn+1, 1);

        RawVolt[chn] = new QLabel;
        RawVolt[chn]->setStyleSheet("QLabel { min-width: 50px ; max-width: 50px }");
        layout->addWidget(RawVolt[chn], chn+1, 2);

        MesVolt[chn] = new QLabel;
        MesVolt[chn]->setStyleSheet("QLabel { min-width: 50px ; max-width: 50px }");
        layout->addWidget(MesVolt[chn], chn+1, 3);

        // group buttons by channel
        vLevels[chn] = new QButtonGroup();

        vLvlBtn[-99][chn] = new QPushButton("off");
        vLvlBtn[  0][chn] = new QPushButton("0v");
        vLvlBtn[  1][chn] = new QPushButton("1v");
        vLvlBtn[  2][chn] = new QPushButton("2.5v");
        vLvlBtn[  5][chn] = new QPushButton("5v");
        vLvlBtn[ 10][chn] = new QPushButton("10v");
        vLvlBtn[-10][chn] = new QPushButton("-10v");

        list<int>::iterator l;
        int column = 4;
        for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++) {

            layout->addWidget(vLvlBtn[*l][chn], chn+1, column++);

            vLvlBtn[*l][chn]->setStyleSheet("QPushButton { min-width: 30px ; max-width: 30px }");
            vLvlBtn[*l][chn]->setHidden(true);
            vLevels[chn]->addButton(vLvlBtn[*l][chn]);

            connect(vLvlBtn[*l][chn],  SIGNAL(clicked()),
                    this,              SLOT(TestVoltage()));
        }
    }
//  gridGroupBox->setMinimumSize(450, 600); // width, height
    gridGroupBox->setStyleSheet(" QGroupBox { min-width: 450px }");
    gridGroupBox->setLayout(layout);
}


void TestA2DPage::TestVoltage()
{
    list<int> voltageLevels = acc->GetVoltageLevels();
    list<int>::iterator l;

    for (int chn = 0; chn < numA2DChannels; chn++)
        for ( l = voltageLevels.begin(); l != voltageLevels.end(); l++)

            if((QPushButton *) sender() == vLvlBtn[*l][chn]) {
                acc->setTestVoltage(dsmId, devId);
                emit TestVoltage(chn, *l);
                return;
            }
}


void TestA2DPage::initializePage()
{
    cout << "TestA2DPage::initializePage" << endl;

    calibrator->setTestVoltage();
    acc->setTestVoltage(-1, -1);

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    if (calibrator->setup("acserver", "diag")) return;
    QApplication::restoreOverrideCursor();

    createTree();
    createGrid();

    treeView->setCurrentIndex(treeView->model()->index(0,0));

    connect(this, SIGNAL(TestVoltage(int, int)),
            acc,    SLOT(TestVoltage(int, int)));

    connect(acc,  SIGNAL(updateSelection()),
            this,   SLOT(updateSelection()));

    connect(acc,  SIGNAL(dispVolts()),
            this,   SLOT(dispVolts()));

/* Doesn't exist??  --cjw Aug/2021
    connect(calibrator, SIGNAL(setValue(int)),
            this,         SLOT(paint()) );
*/
    mainLayout = new QHBoxLayout;
    mainLayout->addWidget(treeView);
    mainLayout->addWidget(gridGroupBox);

    setLayout(mainLayout);

    calibrator->start();  // see Calibrator::run
}
