///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

// FIXME: FCD is handled very badly!

#include <QDebug>
#include <string.h>
#include <errno.h>

#include "SWGDeviceSettings.h"
#include "SWGDeviceState.h"

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/filerecord.h"
#include "fcdproinput.h"

#include <device/devicesourceapi.h>

#include "fcdprothread.h"
#include "fcdtraits.h"
#include "fcdproconst.h"

MESSAGE_CLASS_DEFINITION(FCDProInput::MsgConfigureFCDPro, Message)
MESSAGE_CLASS_DEFINITION(FCDProInput::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(FCDProInput::MsgFileRecord, Message)

FCDProInput::FCDProInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
	m_dev(0),
	m_settings(),
	m_FCDThread(0),
	m_deviceDescription(fcd_traits<Pro>::displayedName),
	m_running(false)
{
    openDevice();
    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

FCDProInput::~FCDProInput()
{
    if (m_running) stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    closeDevice();
}

void FCDProInput::destroy()
{
    delete this;
}

bool FCDProInput::openDevice()
{
    int device = m_deviceAPI->getSampleSourceSequence();

    if (m_dev != 0)
    {
        closeDevice();
    }

    qDebug() << "FCDProInput::openDevice with device #" << device;

    m_dev = fcdOpen(fcd_traits<Pro>::vendorId, fcd_traits<Pro>::productId, device);

    if (m_dev == 0)
    {
        qCritical("FCDProInput::start: could not open FCD");
        return false;
    }

    return true;
}

void FCDProInput::init()
{
    applySettings(m_settings, true);
}

bool FCDProInput::start()
{
	qDebug() << "FCDProInput::start";

//	QMutexLocker mutexLocker(&m_mutex);

    if (!m_dev) {
        return false;
    }

    if (m_running) stop();

	/* Apply settings before streaming to avoid bus contention;
	 * there is very little spare bandwidth on a full speed USB device.
	 * Failure is harmless if no device is found
	 * ... This is rubbish...*/

	//applySettings(m_settings, true);

	if(!m_sampleFifo.setSize(96000*4))
	{
		qCritical("Could not allocate SampleFifo");
		return false;
	}

	m_FCDThread = new FCDProThread(&m_sampleFifo);
	m_FCDThread->startWork();

//	mutexLocker.unlock();
	applySettings(m_settings, true);

	qDebug("FCDProInput::started");
	m_running = true;

	return true;
}

void FCDProInput::closeDevice()
{
    if (m_dev == 0) { // was never open
        return;
    }

    fcdClose(m_dev);
    m_dev = 0;
}

void FCDProInput::stop()
{
//	QMutexLocker mutexLocker(&m_mutex);

	if (m_FCDThread)
	{
		m_FCDThread->stopWork();
		// wait for thread to quit ?
		delete m_FCDThread;
		m_FCDThread = 0;
	}

	m_running = false;
}

QByteArray FCDProInput::serialize() const
{
    return m_settings.serialize();
}

bool FCDProInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureFCDPro* message = MsgConfigureFCDPro::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFCDPro* messageToGUI = MsgConfigureFCDPro::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& FCDProInput::getDeviceDescription() const
{
	return m_deviceDescription;
}

int FCDProInput::getSampleRate() const
{
	return fcd_traits<Pro>::sampleRate;
}

quint64 FCDProInput::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void FCDProInput::setCenterFrequency(qint64 centerFrequency)
{
    FCDProSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency;

    MsgConfigureFCDPro* message = MsgConfigureFCDPro::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureFCDPro* messageToGUI = MsgConfigureFCDPro::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

bool FCDProInput::handleMessage(const Message& message)
{
	if(MsgConfigureFCDPro::match(message))
	{
		qDebug() << "FCDProInput::handleMessage: MsgConfigureFCD";
		MsgConfigureFCDPro& conf = (MsgConfigureFCDPro&) message;
		applySettings(conf.getSettings(), conf.getForce());
		return true;
	}
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "FCDProInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

        if (cmd.getStartStop())
        {
            if (m_deviceAPI->initAcquisition())
            {
                m_deviceAPI->startAcquisition();
            }
        }
        else
        {
            m_deviceAPI->stopAcquisition();
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "FCDProInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

        if (conf.getStartStop())
        {
            if (m_settings.m_fileRecordName.size() != 0) {
                m_fileSink->setFileName(m_settings.m_fileRecordName);
            } else {
                m_fileSink->genUniqueFileName(m_deviceAPI->getDeviceUID());
            }

            m_fileSink->startRecording();
        }
        else
        {
            m_fileSink->stopRecording();
        }

        return true;
    }
	else
	{
		return false;
	}
}

void FCDProInput::applySettings(const FCDProSettings& settings, bool force)
{
	bool forwardChange = false;

	if (force || (m_settings.m_centerFrequency != settings.m_centerFrequency)
            || (m_settings.m_transverterMode != settings.m_transverterMode)
            || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency))
	{
        qint64 deviceCenterFrequency = settings.m_centerFrequency;
        deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
        deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

        if (m_dev != 0)
        {
            set_center_freq((double) deviceCenterFrequency);
        }

        qDebug() << "FCDProInput::applySettings: center freq: " << settings.m_centerFrequency << " Hz"
                << " device center freq: " << deviceCenterFrequency << " Hz";

		forwardChange = (m_settings.m_centerFrequency != settings.m_centerFrequency) || force;

		m_settings.m_centerFrequency = settings.m_centerFrequency;
	}

	if ((m_settings.m_lnaGainIndex != settings.m_lnaGainIndex) || force)
	{
		m_settings.m_lnaGainIndex = settings.m_lnaGainIndex;

		if (m_dev != 0)
		{
			set_lnaGain(settings.m_lnaGainIndex);
		}
	}

	if ((m_settings.m_rfFilterIndex != settings.m_rfFilterIndex) || force)
	{
		m_settings.m_rfFilterIndex = settings.m_rfFilterIndex;

		if (m_dev != 0)
		{
			set_rfFilter(settings.m_rfFilterIndex);
		}
	}

	if ((m_settings.m_lnaEnhanceIndex != settings.m_lnaEnhanceIndex) || force)
	{
		m_settings.m_lnaEnhanceIndex = settings.m_lnaEnhanceIndex;

		if (m_dev != 0)
		{
			set_lnaEnhance(settings.m_lnaEnhanceIndex);
		}
	}

	if ((m_settings.m_bandIndex != settings.m_bandIndex) || force)
	{
		m_settings.m_bandIndex = settings.m_bandIndex;

		if (m_dev != 0)
		{
			set_band(settings.m_bandIndex);
		}
	}

	if ((m_settings.m_mixerGainIndex != settings.m_mixerGainIndex) || force)
	{
		m_settings.m_mixerGainIndex = settings.m_mixerGainIndex;

		if (m_dev != 0)
		{
			set_mixerGain(settings.m_mixerGainIndex);
		}
	}

	if ((m_settings.m_mixerFilterIndex != settings.m_mixerFilterIndex) || force)
	{
		m_settings.m_mixerFilterIndex = settings.m_mixerFilterIndex;

		if (m_dev != 0)
		{
			set_mixerFilter(settings.m_mixerFilterIndex);
		}
	}

	if ((m_settings.m_biasCurrentIndex != settings.m_biasCurrentIndex) || force)
	{
		m_settings.m_biasCurrentIndex = settings.m_biasCurrentIndex;

		if (m_dev != 0)
		{
			set_biasCurrent(settings.m_biasCurrentIndex);
		}
	}

	if ((m_settings.m_modeIndex != settings.m_modeIndex) || force)
	{
		m_settings.m_modeIndex = settings.m_modeIndex;

		if (m_dev != 0)
		{
			set_mode(settings.m_modeIndex);
		}
	}

	if ((m_settings.m_gain1Index != settings.m_gain1Index) || force)
	{
		m_settings.m_gain1Index = settings.m_gain1Index;

		if (m_dev != 0)
		{
			set_gain1(settings.m_gain1Index);
		}
	}

	if ((m_settings.m_rcFilterIndex != settings.m_rcFilterIndex) || force)
	{
		m_settings.m_rcFilterIndex = settings.m_rcFilterIndex;

		if (m_dev != 0)
		{
			set_rcFilter(settings.m_rcFilterIndex);
		}
	}

	if ((m_settings.m_gain2Index != settings.m_gain2Index) || force)
	{
		m_settings.m_gain2Index = settings.m_gain2Index;

		if (m_dev != 0)
		{
			set_gain2(settings.m_gain2Index);
		}
	}

	if ((m_settings.m_gain3Index != settings.m_gain3Index) || force)
	{
		m_settings.m_gain3Index = settings.m_gain3Index;

		if (m_dev != 0)
		{
			set_gain3(settings.m_gain3Index);
		}
	}

	if ((m_settings.m_gain4Index != settings.m_gain4Index) || force)
	{
		m_settings.m_gain4Index = settings.m_gain4Index;

		if (m_dev != 0)
		{
			set_gain4(settings.m_gain4Index);
		}
	}

	if ((m_settings.m_ifFilterIndex != settings.m_ifFilterIndex) || force)
	{
		m_settings.m_ifFilterIndex = settings.m_ifFilterIndex;

		if (m_dev != 0)
		{
			set_ifFilter(settings.m_ifFilterIndex);
		}
	}

	if ((m_settings.m_gain5Index != settings.m_gain5Index) || force)
	{
		m_settings.m_gain5Index = settings.m_gain5Index;

		if (m_dev != 0)
		{
			set_gain5(settings.m_gain5Index);
		}
	}

	if ((m_settings.m_gain6Index != settings.m_gain6Index) || force)
	{
		m_settings.m_gain6Index = settings.m_gain6Index;

		if (m_dev != 0)
		{
			set_gain6(settings.m_gain6Index);
		}
	}

	if ((m_settings.m_LOppmTenths != settings.m_LOppmTenths) || force)
	{
		m_settings.m_LOppmTenths = settings.m_LOppmTenths;

		if (m_dev != 0)
		{
			set_lo_ppm();
		}
	}

	if ((m_settings.m_dcBlock != settings.m_dcBlock) || force)
	{
		m_settings.m_dcBlock = settings.m_dcBlock;
		m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
	}

	if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
	{
		m_settings.m_iqCorrection = settings.m_iqCorrection;
		m_deviceAPI->configureCorrections(m_settings.m_dcBlock, m_settings.m_iqCorrection);
	}

    if (forwardChange)
    {
		DSPSignalNotification *notif = new DSPSignalNotification(fcd_traits<Pro>::sampleRate, m_settings.m_centerFrequency);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }
}

void FCDProInput::set_center_freq(double freq)
{
	freq += freq*(((double) m_settings.m_LOppmTenths)/10000000.0);

	if (fcdAppSetFreq(m_dev, freq) == FCD_MODE_NONE)
	{
		qDebug("No FCD HID found for frquency change");
	}

}

void FCDProInput::set_bias_t(bool on __attribute__((unused)))
{
	//quint8 cmd = on ? 1 : 0;

	// TODO: use FCD Pro controls
	//fcdAppSetParam(m_dev, FCD_CMD_APP_SET_BIAS_TEE, &cmd, 1);
}

void FCDProInput::set_lnaGain(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_lna_gain_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::lna_gains[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_LNA_GAIN, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_lnaGain: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_rfFilter(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_rf_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::rf_filters[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_RF_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_rfFilter: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_lnaEnhance(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_lna_enhance_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::lna_enhances[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_LNA_ENHANCE, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_lnaEnhance: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_band(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_band_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::bands[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_BAND, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_band: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_mixerGain(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_mixer_gain_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::mixer_gains[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_MIXER_GAIN, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_mixerGain: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_mixerFilter(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_mixer_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::mixer_filters[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_MIXER_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_mixerFilter: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_biasCurrent(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_bias_current_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::bias_currents[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_BIAS_CURRENT, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_biasCurrent: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_mode(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain_mode_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gain_modes[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN_MODE, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_mode: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain1(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain1_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains1[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN1, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain1: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_rcFilter(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_rc_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_rc_filters[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_RC_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_rcFilter: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain2(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain2_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains2[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN2, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain2: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain3(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain3_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains3[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN3, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain3: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain4(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain4_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains4[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN4, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain4: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_ifFilter(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_filter_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_filters[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_FILTER, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_ifFilter: failed to set at " << cmd_value;
	}

}

void FCDProInput::set_gain5(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain5_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains5[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN5, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain5: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_gain6(int index)
{
	if ((index < 0) || (index >= FCDProConstants::fcdpro_if_gain6_nb_values()))
	{
		return;
	}

	quint8 cmd_value = FCDProConstants::if_gains6[index].value;

	if (fcdAppSetParam(m_dev, FCDPRO_HID_CMD_SET_IF_GAIN6, &cmd_value, 1) != FCD_MODE_APP)
	{
		qWarning() << "FCDProPlusInput::set_gain6: failed to set at " << cmd_value;
	}
}

void FCDProInput::set_lo_ppm()
{
	set_center_freq((double) m_settings.m_centerFrequency);
}

int FCDProInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int FCDProInput::webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    MsgStartStop *message = MsgStartStop::create(run);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgStartStop *msgToGUI = MsgStartStop::create(run);
        m_guiMessageQueue->push(msgToGUI);
    }

    return 200;
}

int FCDProInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setFcdProSettings(new SWGSDRangel::SWGFCDProSettings());
    response.getFcdProSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int FCDProInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    FCDProSettings settings = m_settings;

    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getFcdProSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("LOppmTenths")) {
        settings.m_LOppmTenths = response.getFcdProSettings()->getLOppmTenths();
    }
    if (deviceSettingsKeys.contains("lnaGainIndex")) {
        settings.m_lnaGainIndex = response.getFcdProSettings()->getLnaGainIndex();
    }
    if (deviceSettingsKeys.contains("rfFilterIndex")) {
        settings.m_rfFilterIndex = response.getFcdProSettings()->getRfFilterIndex();
    }
    if (deviceSettingsKeys.contains("lnaEnhanceIndex")) {
        settings.m_lnaEnhanceIndex = response.getFcdProSettings()->getLnaEnhanceIndex();
    }
    if (deviceSettingsKeys.contains("bandIndex")) {
        settings.m_bandIndex = response.getFcdProSettings()->getBandIndex();
    }
    if (deviceSettingsKeys.contains("mixerGainIndex")) {
        settings.m_mixerGainIndex = response.getFcdProSettings()->getMixerGainIndex();
    }
    if (deviceSettingsKeys.contains("mixerFilterIndex")) {
        settings.m_mixerFilterIndex = response.getFcdProSettings()->getMixerFilterIndex();
    }
    if (deviceSettingsKeys.contains("biasCurrentIndex")) {
        settings.m_biasCurrentIndex = response.getFcdProSettings()->getBiasCurrentIndex();
    }
    if (deviceSettingsKeys.contains("modeIndex")) {
        settings.m_modeIndex = response.getFcdProSettings()->getModeIndex();
    }
    if (deviceSettingsKeys.contains("gain1Index")) {
        settings.m_gain1Index = response.getFcdProSettings()->getGain1Index();
    }
    if (deviceSettingsKeys.contains("gain2Index")) {
        settings.m_gain2Index = response.getFcdProSettings()->getGain2Index();
    }
    if (deviceSettingsKeys.contains("gain3Index")) {
        settings.m_gain3Index = response.getFcdProSettings()->getGain3Index();
    }
    if (deviceSettingsKeys.contains("gain4Index")) {
        settings.m_gain4Index = response.getFcdProSettings()->getGain4Index();
    }
    if (deviceSettingsKeys.contains("gain5Index")) {
        settings.m_gain5Index = response.getFcdProSettings()->getGain5Index();
    }
    if (deviceSettingsKeys.contains("gain6Index")) {
        settings.m_gain6Index = response.getFcdProSettings()->getGain6Index();
    }
    if (deviceSettingsKeys.contains("rcFilterIndex")) {
        settings.m_rcFilterIndex = response.getFcdProSettings()->getRcFilterIndex();
    }
    if (deviceSettingsKeys.contains("ifFilterIndex")) {
        settings.m_ifFilterIndex = response.getFcdProSettings()->getIfFilterIndex();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getFcdProSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getFcdProSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getFcdProSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getFcdProSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getFcdProSettings()->getFileRecordName();
    }

    MsgConfigureFCDPro *msg = MsgConfigureFCDPro::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFCDPro *msgToGUI = MsgConfigureFCDPro::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void FCDProInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const FCDProSettings& settings)
{
    response.getFcdProSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getFcdProSettings()->setLOppmTenths(settings.m_LOppmTenths);
    response.getFcdProSettings()->setLnaGainIndex(settings.m_lnaGainIndex);
    response.getFcdProSettings()->setRfFilterIndex(settings.m_rfFilterIndex);
    response.getFcdProSettings()->setLnaEnhanceIndex(settings.m_lnaEnhanceIndex);
    response.getFcdProSettings()->setBandIndex(settings.m_bandIndex);
    response.getFcdProSettings()->setMixerGainIndex(settings.m_mixerGainIndex);
    response.getFcdProSettings()->setMixerFilterIndex(settings.m_mixerFilterIndex);
    response.getFcdProSettings()->setBiasCurrentIndex(settings.m_biasCurrentIndex);
    response.getFcdProSettings()->setModeIndex(settings.m_modeIndex);
    response.getFcdProSettings()->setGain1Index(settings.m_gain1Index);
    response.getFcdProSettings()->setGain2Index(settings.m_gain2Index);
    response.getFcdProSettings()->setGain3Index(settings.m_gain3Index);
    response.getFcdProSettings()->setGain4Index(settings.m_gain4Index);
    response.getFcdProSettings()->setGain5Index(settings.m_gain5Index);
    response.getFcdProSettings()->setGain6Index(settings.m_gain6Index);
    response.getFcdProSettings()->setRcFilterIndex(settings.m_rcFilterIndex);
    response.getFcdProSettings()->setIfFilterIndex(settings.m_ifFilterIndex);
    response.getFcdProSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getFcdProSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getFcdProSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getFcdProSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    if (response.getFcdProSettings()->getFileRecordName()) {
        *response.getFcdProSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getFcdProSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }
}
