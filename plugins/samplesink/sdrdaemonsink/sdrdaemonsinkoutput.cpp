///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <errno.h>
#include <QDebug>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGSDRdaemonSinkReport.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"

#include "device/devicesinkapi.h"

#include "sdrdaemonsinkoutput.h"
#include "sdrdaemonsinkthread.h"

MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkWork, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkChunkCorrection, Message)
MESSAGE_CLASS_DEFINITION(SDRdaemonSinkOutput::MsgReportSDRdaemonSinkStreamTiming, Message)

SDRdaemonSinkOutput::SDRdaemonSinkOutput(DeviceSinkAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_settings(),
    m_sdrDaemonSinkThread(0),
	m_deviceDescription("SDRdaemonSink"),
    m_startingTimeStamp(0),
	m_masterTimer(deviceAPI->getMasterTimer())
{
}

SDRdaemonSinkOutput::~SDRdaemonSinkOutput()
{
	stop();
}

void SDRdaemonSinkOutput::destroy()
{
    delete this;
}

bool SDRdaemonSinkOutput::start()
{
	QMutexLocker mutexLocker(&m_mutex);
	qDebug() << "SDRdaemonSinkOutput::start";

	m_sdrDaemonSinkThread = new SDRdaemonSinkThread(&m_sampleSourceFifo);
	m_sdrDaemonSinkThread->setRemoteAddress(m_settings.m_address, m_settings.m_dataPort);
	m_sdrDaemonSinkThread->setCenterFrequency(m_settings.m_centerFrequency);
	m_sdrDaemonSinkThread->setSamplerate(m_settings.m_sampleRate);
	m_sdrDaemonSinkThread->setNbBlocksFEC(m_settings.m_nbFECBlocks);
	m_sdrDaemonSinkThread->connectTimer(m_masterTimer);
	m_sdrDaemonSinkThread->startWork();

    double delay = ((127*127*m_settings.m_txDelay) / m_settings.m_sampleRate)/(128 + m_settings.m_nbFECBlocks);
    m_sdrDaemonSinkThread->setTxDelay((int) (delay*1e6));

	mutexLocker.unlock();
	//applySettings(m_generalSettings, m_settings, true);
	qDebug("SDRdaemonSinkOutput::start: started");

	return true;
}

void SDRdaemonSinkOutput::init()
{
    applySettings(m_settings, true);
}

void SDRdaemonSinkOutput::stop()
{
	qDebug() << "SDRdaemonSinkOutput::stop";
	QMutexLocker mutexLocker(&m_mutex);

	if(m_sdrDaemonSinkThread != 0)
	{
	    m_sdrDaemonSinkThread->stopWork();
		delete m_sdrDaemonSinkThread;
		m_sdrDaemonSinkThread = 0;
	}
}

QByteArray SDRdaemonSinkOutput::serialize() const
{
    return m_settings.serialize();
}

bool SDRdaemonSinkOutput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureSDRdaemonSink* message = MsgConfigureSDRdaemonSink::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRdaemonSink* messageToGUI = MsgConfigureSDRdaemonSink::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& SDRdaemonSinkOutput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int SDRdaemonSinkOutput::getSampleRate() const
{
	return m_settings.m_sampleRate;
}

quint64 SDRdaemonSinkOutput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void SDRdaemonSinkOutput::setCenterFrequency(qint64 centerFrequency)
{
    SDRdaemonSinkSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureSDRdaemonSink* message = MsgConfigureSDRdaemonSink::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureSDRdaemonSink* messageToGUI = MsgConfigureSDRdaemonSink::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::time_t SDRdaemonSinkOutput::getStartingTimeStamp() const
{
	return m_startingTimeStamp;
}

bool SDRdaemonSinkOutput::handleMessage(const Message& message)
{

    if (MsgConfigureSDRdaemonSink::match(message))
    {
        qDebug() << "SDRdaemonSinkOutput::handleMessage:" << message.getIdentifier();
	    MsgConfigureSDRdaemonSink& conf = (MsgConfigureSDRdaemonSink&) message;
        applySettings(conf.getSettings(), conf.getForce());
        return true;
    }
	else if (MsgConfigureSDRdaemonSinkWork::match(message))
	{
		MsgConfigureSDRdaemonSinkWork& conf = (MsgConfigureSDRdaemonSinkWork&) message;
		bool working = conf.isWorking();

		if (m_sdrDaemonSinkThread != 0)
		{
			if (working)
			{
			    m_sdrDaemonSinkThread->startWork();
			}
			else
			{
			    m_sdrDaemonSinkThread->stopWork();
			}
		}

		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "SDRdaemonSinkOutput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initGeneration())
            {
                m_deviceAPI->startGeneration();
            }
        }
        else
        {
            m_deviceAPI->stopGeneration();
        }

        return true;
    }
	else if (MsgConfigureSDRdaemonSinkStreamTiming::match(message))
	{
        MsgReportSDRdaemonSinkStreamTiming *report;

		if (m_sdrDaemonSinkThread != 0 && getMessageQueueToGUI())
		{
			report = MsgReportSDRdaemonSinkStreamTiming::create(m_sdrDaemonSinkThread->getSamplesCount());
			getMessageQueueToGUI()->push(report);
		}

		return true;
	}
	else if (MsgConfigureSDRdaemonSinkChunkCorrection::match(message))
	{
	    MsgConfigureSDRdaemonSinkChunkCorrection& conf = (MsgConfigureSDRdaemonSinkChunkCorrection&) message;

	    if (m_sdrDaemonSinkThread != 0)
        {
	        m_sdrDaemonSinkThread->setChunkCorrection(conf.getChunkCorrection());
        }

	    return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonSinkOutput::applySettings(const SDRdaemonSinkSettings& settings, bool force)
{
    QMutexLocker mutexLocker(&m_mutex);
    bool forwardChange = false;
    bool changeTxDelay = false;

    if (force || (m_settings.m_address != settings.m_address) || (m_settings.m_dataPort != settings.m_dataPort))
    {
        m_settings.m_address = settings.m_address;
        m_settings.m_dataPort = settings.m_dataPort;

        if (m_sdrDaemonSinkThread != 0)
        {
            m_sdrDaemonSinkThread->setRemoteAddress(m_settings.m_address, m_settings.m_dataPort);
        }
    }

    if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency))
    {
        m_settings.m_centerFrequency = settings.m_centerFrequency;

        if (m_sdrDaemonSinkThread != 0)
        {
            m_sdrDaemonSinkThread->setCenterFrequency(m_settings.m_centerFrequency);
        }

        forwardChange = true;
    }

    if (force || (m_settings.m_sampleRate != settings.m_sampleRate))
    {
        m_settings.m_sampleRate = settings.m_sampleRate;

        if (m_sdrDaemonSinkThread != 0)
        {
            m_sdrDaemonSinkThread->setSamplerate(m_settings.m_sampleRate);
        }

        forwardChange = true;
        changeTxDelay = true;
    }

    if (force || (m_settings.m_log2Interp != settings.m_log2Interp))
    {
        m_settings.m_log2Interp = settings.m_log2Interp;
        forwardChange = true;
    }

    if (force || (m_settings.m_nbFECBlocks != settings.m_nbFECBlocks))
    {
        m_settings.m_nbFECBlocks = settings.m_nbFECBlocks;

        if (m_sdrDaemonSinkThread != 0)
        {
            m_sdrDaemonSinkThread->setNbBlocksFEC(m_settings.m_nbFECBlocks);
        }

        changeTxDelay = true;
    }

    if (force || (m_settings.m_txDelay != settings.m_txDelay))
    {
        m_settings.m_txDelay = settings.m_txDelay;
        changeTxDelay = true;
    }

    if (changeTxDelay)
    {
        double delay = ((127*127*m_settings.m_txDelay) / m_settings.m_sampleRate)/(128 + m_settings.m_nbFECBlocks);
        qDebug("SDRdaemonSinkOutput::applySettings: Tx delay: %f us", delay*1e6);

        if (m_sdrDaemonSinkThread != 0)
        {
            // delay is calculated as a fraction of the nominal UDP block process time
            // frame size: 127 * 127 samples
            // divided by sample rate gives the frame process time
            // divided by the number of actual blocks including FEC blocks gives the block (i.e. UDP block) process time
            m_sdrDaemonSinkThread->setTxDelay((int) (delay*1e6));
        }
    }

    mutexLocker.unlock();

    qDebug("SDRdaemonSinkOutput::applySettings: %s m_centerFrequency: %llu m_sampleRate: %llu m_log2Interp: %d m_txDelay: %f m_nbFECBlocks: %d",
            forwardChange ? "forward change" : "",
            m_settings.m_centerFrequency,
            m_settings.m_sampleRate,
            m_settings.m_log2Interp,
            m_settings.m_txDelay,
            m_settings.m_nbFECBlocks);

    if (forwardChange)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(m_settings.m_sampleRate, m_settings.m_centerFrequency);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

int SDRdaemonSinkOutput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int SDRdaemonSinkOutput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgStartStop *messagetoGui = MsgStartStop::create(run);
        m_guiMessageQueue->push(messagetoGui);
    }

    return 200;
}

int SDRdaemonSinkOutput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setSdrDaemonSinkSettings(new SWGSDRangel::SWGSDRdaemonSinkSettings());
    response.getSdrDaemonSinkSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int SDRdaemonSinkOutput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    SDRdaemonSinkSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getSdrDaemonSinkSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getSdrDaemonSinkSettings()->getSampleRate();
    }
    if (deviceSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getSdrDaemonSinkSettings()->getLog2Interp();
    }
    if (deviceSettingsKeys.contains("txDelay")) {
        settings.m_txDelay = response.getSdrDaemonSinkSettings()->getTxDelay();
    }
    if (deviceSettingsKeys.contains("nbFECBlocks")) {
        settings.m_nbFECBlocks = response.getSdrDaemonSinkSettings()->getNbFecBlocks();
    }
    if (deviceSettingsKeys.contains("address")) {
        settings.m_address = *response.getSdrDaemonSinkSettings()->getAddress();
    }
    if (deviceSettingsKeys.contains("dataPort")) {
        settings.m_dataPort = response.getSdrDaemonSinkSettings()->getDataPort();
    }
    if (deviceSettingsKeys.contains("controlPort")) {
        settings.m_controlPort = response.getSdrDaemonSinkSettings()->getControlPort();
    }
    if (deviceSettingsKeys.contains("specificParameters")) {
        settings.m_specificParameters = *response.getSdrDaemonSinkSettings()->getSpecificParameters();
    }

    MsgConfigureSDRdaemonSink *msg = MsgConfigureSDRdaemonSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSDRdaemonSink *msgToGUI = MsgConfigureSDRdaemonSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

int SDRdaemonSinkOutput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setSdrDaemonSinkReport(new SWGSDRangel::SWGSDRdaemonSinkReport());
    response.getSdrDaemonSinkReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

void SDRdaemonSinkOutput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SDRdaemonSinkSettings& settings)
{
    response.getSdrDaemonSinkSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getSdrDaemonSinkSettings()->setSampleRate(settings.m_sampleRate);
    response.getSdrDaemonSinkSettings()->setLog2Interp(settings.m_log2Interp);
    response.getSdrDaemonSinkSettings()->setTxDelay(settings.m_txDelay);
    response.getSdrDaemonSinkSettings()->setNbFecBlocks(settings.m_nbFECBlocks);
    response.getSdrDaemonSinkSettings()->setAddress(new QString(settings.m_address));
    response.getSdrDaemonSinkSettings()->setDataPort(settings.m_dataPort);
    response.getSdrDaemonSinkSettings()->setControlPort(settings.m_controlPort);
    response.getSdrDaemonSinkSettings()->setSpecificParameters(new QString(settings.m_specificParameters));
}

void SDRdaemonSinkOutput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    response.getSdrDaemonSinkReport()->setBufferRwBalance(m_sampleSourceFifo.getRWBalance());
    response.getSdrDaemonSinkReport()->setSampleCount(m_sdrDaemonSinkThread ? (int) m_sdrDaemonSinkThread->getSamplesCount() : 0);
}


