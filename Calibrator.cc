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
#include "Calibrator.h"
#include "AutoCalClient.h"

#include <sys/stat.h>

#include <nidas/core/Socket.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/IOChannel.h>
#include <nidas/core/Project.h>
#include <nidas/core/requestXMLConfig.h>

#include <nidas/util/EOFException.h>
#include <nidas/util/Process.h>
#include <nidas/util/auto_ptr.h>

#include <nidas/dynld/raf/DSMAnalogSensor.h>

#ifdef SIMULATE
#include <nidas/core/FileSet.h>
#endif

#include <QMessageBox>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

string stateEnumDesc[] = {"GATHER", "DONE", "DEAD" };

class AutoProject
{
public:
    AutoProject() { Project::getInstance(); }
    ~AutoProject() { Project::destroyInstance(); }
};

Calibrator::Calibrator( AutoCalClient *acc ):
   _testVoltage(false),
   _canceled(false),
   _acc(acc),
   _sis(0),
   _pipeline(0)
{
    AutoProject project;
}


Calibrator::~Calibrator()
{
    cout << "Calibrator::~Calibrator" << endl;

    if (_pipeline)
        _pipeline->getProcessedSampleSource()->removeSampleClient(_acc);

    if (isRunning()) {
        cancel();
        wait();
    }
    delete _sis;
    delete _pipeline;
};


bool Calibrator::setup(QString host, QString mode)
{
    cout << "Calibrator::setup(), mode=[" << mode.toStdString() << "]\n";

    try {
        IOChannel* iochan = 0;

#ifdef SIMULATE
        // local data file is a copy of:
        // /scr/raf/Raw_Data/PLOWS/20091106_161332_ff04.ads
        list<string> dataFileNames;
        dataFileNames.push_back("/home/data/Raw_Data/20210529_132233_rf01.ads");
        nidas::core::FileSet* fset =
            nidas::core::FileSet::getFileSet(dataFileNames);
        iochan = fset->connect();
        cout << "SIMULATE!  using " << dataFileNames.front() << endl;
#else
        // real time operation
        cout << "hostName: " << host.toStdString() << endl;
        n_u::Socket * sock = new n_u::Socket(host.toStdString(), NIDAS_SVC_REQUEST_PORT_UDP);
        iochan = new nidas::core::Socket(sock);
        cout << "Calibrator::setup() connected to dsm_server" << endl;
#endif

        _sis = new RawSampleInputStream(iochan); // RawSampleStream now owns the iochan ptr.
        _sis->setMaxSampleLength(32768);

        cout << "Calibrator::setup() RawSampleStream now owns the iochan ptr." << endl;

        // Address to use when fishing for the XML configuration.
        n_u::Inet4SocketAddress _configSockAddr;
        try {
            _configSockAddr = n_u::Inet4SocketAddress(
              n_u::Inet4Address::getByName(host.toStdString()),
              NIDAS_SVC_REQUEST_PORT_UDP);
        }
        catch(const n_u::UnknownHostException& e) {      // shouldn't happen
            ostringstream ostr;
            ostr << "Failed to aquire XML configuration: " << e.what();
            cout << ostr.str() << endl;
            QMessageBox::critical(0, "CANNOT start", ostr.str().c_str());
            return true;
        }
        // Pull in the XML configuration from the DSM server.
        n_u::auto_ptr<xercesc::DOMDocument> doc(requestXMLConfig(true,_configSockAddr));

        Project::getInstance()->fromDOMElement(doc->getDocumentElement());
        doc.release();

        bool noneFound = true;

        _pipeline = new SamplePipeline();
        cout << "_pipeline: " << _pipeline << endl;

        DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
        for ( ; di.hasNext(); ) {
            const DSMConfig* dsm = di.next();
            const list<DSMSensor*>& allSensors = dsm->getSensors();

            list<DSMSensor*>::const_iterator si;
            for (si = allSensors.begin(); si != allSensors.end(); ++si) {
                DSMSensor* sensor = *si;

                if (_canceled)
                    return true;

                // skip non-Analog type sensors
                // Cal mode is for ncar_a2d only.  Diag nostic mode is for all
                if (mode == "cal" && sensor->getClassName().compare("raf.DSMAnalogSensor"))
                    continue;
                if (sensor->getClassName().compare("raf.DSMAnalogSensor") &&    // ncar_a2d
                    sensor->getClassName().compare("DSC_A2DSensor") &&  // Diamond - ddmat
                    sensor->getClassName().compare("raf.A2D_Serial"))   // gpDAQ
                    continue;

                // skip non-responsive of miss-configured sensors
                if ( _acc->Setup(sensor) )
                    continue;

                dsmLocations[dsm->getId()] = dsm->getLocation();

                // DEBUG - print out the found calibration coeffients
                uint dsmId = sensor->getDSMId();
                uint devId = sensor->getSensorId();
                for (uint chn = 0; chn < 8; chn++) {
                    if (_acc->GetOldCals(dsmId, devId, chn).size() > 1) {
                        cout << __PRETTY_FUNCTION__;
                        cout << " dsmId: " << dsmId << " devId: " << devId << " chn: " << chn;
                        cout << " nCals: " << _acc->GetOldCals(dsmId, devId, chn).size();
                        cout << " Intcp: " << _acc->GetOldCals(dsmId, devId, chn)[0];
                        cout << " Slope: " << _acc->GetOldCals(dsmId, devId, chn)[1];
                        cout << endl;
                    }
                }
                // default slopes and intersects to 1.0 and 0.0
                sensor->removeCalFiles();

                // initialize the sensor
                sensor->init();

                //  inform the SampleInputStream of what SampleTags to expect
                cout << "_sis->addSampleTag(sensor->getRawSampleTag());" << endl;
                _sis->addSampleTag(sensor->getRawSampleTag());

                // connect to the _pipeline member
                _pipeline->connect(sensor);

                noneFound = false;
            }
        }
        if ( noneFound ) {
            ostringstream ostr;
            ostr << "No analog cards available to calibrate!";
            cout << ostr.str() << endl;
            QMessageBox::critical(0, "no cards", ostr.str().c_str());
            return true;
        }
        cout << "Calibrator::setup() extracted analog sensors" << endl;
        _acc->createQtTreeModel(dsmLocations);
    }
    catch (n_u::IOException& e) {
        cout << "DSM server is not running!" << endl;
        cout << "You need to start NIDAS" << endl;
        return true;
    }
    cout << "Calibrator::setup() FINISHED" << endl;
    return false;
}


void Calibrator::run()
{
    cout << "Calibrator::run()" << endl;

    try {
        _pipeline->setRealTime(true);
        _pipeline->setProcSorterLength(0);

        // 2. connect the pipeline to the SampleInputStream.
        _pipeline->connect(_sis);

        // 3. connect the client to the pipeline
        _pipeline->getProcessedSampleSource()->addSampleClient(_acc);

        try {
            enum stateEnum state = GATHER;
            while (_testVoltage) {
                _sis->readSamples();  // see AutoCalClient::receive
                if (_canceled) {
                    cout << "Canceling diagnostics..." << endl;
                    state = DONE;
                    break;
                }
            }
            while ( (state = _acc->SetNextCalVoltage(state)) != DONE ) {

                cout << "state: " << stateEnumDesc[state] << endl;

                if (state == DONE)
                    break;

                if (state == DEAD)
                    break;

                cout << "gathering..." << endl;
                while ( _testVoltage || !_acc->Gathered() ) {

                    if (_canceled) {
                        cout << "Canceling calibration..." << endl;
                        state = DONE;
                        break;
                    }
                    _sis->readSamples();  // see AutoCalClient::receive

                    // update progress bar
                    if (!_testVoltage)
                        emit setValue(_acc->progress);
                }
            }
            if (state == DONE) {
                _acc->DisplayResults();

                // update progress bar
                emit setValue(_acc->progress);
            }
        }
        catch (n_u::EOFException& e) {
            cerr << e.what() << endl;
        }
        catch (n_u::IOException& e) {
            _pipeline->getProcessedSampleSource()->removeSampleClient(_acc);
            _pipeline->disconnect(_sis);
            _sis->close();
            throw(e);
        }
        _pipeline->getProcessedSampleSource()->removeSampleClient(_acc);
        _pipeline->disconnect(_sis);
        _sis->close();
    }
    catch (n_u::IOException& e) {
        cerr << e.what() << endl;
    }
    catch (n_u::Exception& e) {
        cerr << e.what() << endl;
    }
    cout << "Calibrator::run() FINISHED" << endl;
}


void Calibrator::cancel()
{
    _canceled = true;
}
