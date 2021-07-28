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

#ifndef _TestA2DPage_h_
#define _TestA2DPage_h_

#include <nidas/core/DSMSensor.h>
#include <map>

#include "Calibrator.h"
#include "TreeModel.h"

#include <QItemSelection>
#include <QWizardPage>

namespace n_u = nidas::util;

using namespace nidas::core;
using namespace std;

QT_BEGIN_NAMESPACE
class QButtonGroup;
class QCheckBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTreeView;
class QHBoxLayout;
class QVBoxLayout;
class QWidget;
QT_END_NAMESPACE


class TestA2DPage : public QWizardPage
{
    Q_OBJECT

public:
    TestA2DPage(Calibrator *calibrator, AutoCalClient *acc, QWidget *parent = 0);
    ~TestA2DPage();

    void initializePage();
    int nextId() const { return -1; }

signals:
    void TestVoltage(int channel, int level);

public slots:
    void dispVolts();
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void TestVoltage();
    void updateSelection();

private:
    int dsmId;
    int devId;

    Calibrator *calibrator;

    AutoCalClient *acc;

    void createTree();
    void createGrid();

    QTreeView *treeView;
    TreeModel *treeModel;
    QGroupBox *gridGroupBox;

    QVBoxLayout *treeLayout;
    QButtonGroup *buttonGroup;
    QHBoxLayout *mainLayout;

    enum { numA2DChannels = 8 };

    QLabel *ChannelTitle;
    QLabel *VarNameTitle;
    QLabel *RawVoltTitle;
    QLabel *MesVoltTitle;
    QLabel *SetVoltTitle;

    QLabel *Channel[numA2DChannels];
    QLabel *VarName[numA2DChannels];
    QLabel *RawVolt[numA2DChannels];
    QLabel *MesVolt[numA2DChannels];
    QHBoxLayout *SetVolt[numA2DChannels];

    map<int, map< int, QPushButton* > > vLvlBtn;

    QButtonGroup* vLevels[numA2DChannels];
};

#endif
