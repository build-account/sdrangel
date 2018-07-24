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

#include <QMutexLocker>
#include <QDebug>
#include <cstddef>
#include <string.h>
#include "lime/LimeSuite.h"

#include "SWGDeviceSettings.h"
#include "SWGLimeSdrInputSettings.h"
#include "SWGDeviceState.h"
#include "SWGDeviceReport.h"
#include "SWGLimeSdrInputReport.h"

#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "dsp/dspcommands.h"
#include "dsp/filerecord.h"
#include "dsp/dspengine.h"
#include "limesdrinput.h"
#include "limesdrinputthread.h"
#include "limesdr/devicelimesdrparam.h"
#include "limesdr/devicelimesdrshared.h"
#include "limesdr/devicelimesdr.h"

MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgConfigureLimeSDR, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgGetStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgGetDeviceInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgReportStreamInfo, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgFileRecord, Message)
MESSAGE_CLASS_DEFINITION(LimeSDRInput::MsgStartStop, Message)

LimeSDRInput::LimeSDRInput(DeviceSourceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_limeSDRInputThread(0),
    m_deviceDescription("LimeSDRInput"),
    m_running(false),
    m_channelAcquired(false)
{
    m_streamId.handle = 0;
    suspendRxBuddies();
    suspendTxBuddies();
    openDevice();
    resumeTxBuddies();
    resumeRxBuddies();

    m_fileSink = new FileRecord(QString("test_%1.sdriq").arg(m_deviceAPI->getDeviceUID()));
    m_deviceAPI->addSink(m_fileSink);
}

LimeSDRInput::~LimeSDRInput()
{
    if (m_running) stop();
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    suspendRxBuddies();
    suspendTxBuddies();
    closeDevice();
    resumeTxBuddies();
    resumeRxBuddies();
}

void LimeSDRInput::destroy()
{
    delete this;
}

bool LimeSDRInput::openDevice()
{
    if (!m_sampleFifo.setSize(96000 * 4))
    {
        qCritical("LimeSDRInput::openDevice: could not allocate SampleFifo");
        return false;
    }
    else
    {
        qDebug("LimeSDRInput::openDevice: allocated SampleFifo");
    }

    int requestedChannel = m_deviceAPI->getItemIndex();

    // look for Rx buddies and get reference to common parameters
    // if there is a channel left take the first available
    if (m_deviceAPI->getSourceBuddies().size() > 0) // look source sibling first
    {
        qDebug("LimeSDRInput::openDevice: look in Rx buddies");

        DeviceSourceAPI *sourceBuddy = m_deviceAPI->getSourceBuddies()[0];
        //m_deviceShared = *((DeviceLimeSDRShared *) sourceBuddy->getBuddySharedPtr()); // copy shared data
        DeviceLimeSDRShared *deviceLimeSDRShared = (DeviceLimeSDRShared*) sourceBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = deviceLimeSDRShared->m_deviceParams;

        DeviceLimeSDRParams *deviceParams = m_deviceShared.m_deviceParams; // get device parameters

        if (deviceParams == 0)
        {
            qCritical("LimeSDRInput::openDevice: cannot get device parameters from Rx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("LimeSDRInput::openDevice: getting device parameters from Rx buddy");
        }

        if (m_deviceAPI->getSourceBuddies().size() == deviceParams->m_nbRxChannels)
        {
            qCritical("LimeSDRInput::openDevice: no more Rx channels available in device");
            return false; // no more Rx channels available in device
        }
        else
        {
            qDebug("LimeSDRInput::openDevice: at least one more Rx channel is available in device");
        }

        // check if the requested channel is busy and abort if so (should not happen if device management is working correctly)

        for (unsigned int i = 0; i < m_deviceAPI->getSourceBuddies().size(); i++)
        {
            DeviceSourceAPI *buddy = m_deviceAPI->getSourceBuddies()[i];
            DeviceLimeSDRShared *buddyShared = (DeviceLimeSDRShared *) buddy->getBuddySharedPtr();

            if (buddyShared->m_channel == requestedChannel)
            {
                qCritical("LimeSDRInput::openDevice: cannot open busy channel %u", requestedChannel);
                return false;
            }
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // look for Tx buddies and get reference to common parameters
    // take the first Rx channel
    else if (m_deviceAPI->getSinkBuddies().size() > 0) // then sink
    {
        qDebug("LimeSDRInput::openDevice: look in Tx buddies");

        DeviceSinkAPI *sinkBuddy = m_deviceAPI->getSinkBuddies()[0];
        //m_deviceShared = *((DeviceLimeSDRShared *) sinkBuddy->getBuddySharedPtr()); // copy parameters
        DeviceLimeSDRShared *deviceLimeSDRShared = (DeviceLimeSDRShared*) sinkBuddy->getBuddySharedPtr();
        m_deviceShared.m_deviceParams = deviceLimeSDRShared->m_deviceParams;

        if (m_deviceShared.m_deviceParams == 0)
        {
            qCritical("LimeSDRInput::openDevice: cannot get device parameters from Tx buddy");
            return false; // the device params should have been created by the buddy
        }
        else
        {
            qDebug("LimeSDRInput::openDevice: getting device parameters from Tx buddy");
        }

        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }
    // There are no buddies then create the first LimeSDR common parameters
    // open the device this will also populate common fields
    // take the first Rx channel
    else
    {
        qDebug("LimeSDRInput::openDevice: open device here");

        m_deviceShared.m_deviceParams = new DeviceLimeSDRParams();
        char serial[256];
        strcpy(serial, qPrintable(m_deviceAPI->getSampleSourceSerial()));
        m_deviceShared.m_deviceParams->open(serial);
        m_deviceShared.m_channel = requestedChannel; // acknowledge the requested channel
    }

    m_deviceAPI->setBuddySharedPtr(&m_deviceShared); // propagate common parameters to API

    return true;
}

void LimeSDRInput::suspendRxBuddies()
{
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("LimeSDRInput::suspendRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSource)->getBuddySharedPtr();

        if (buddySharedPtr->m_thread && buddySharedPtr->m_thread->isRunning())
        {
            buddySharedPtr->m_thread->stopWork();
            buddySharedPtr->m_threadWasRunning = true;
        }
        else
        {
            buddySharedPtr->m_threadWasRunning = false;
        }
    }
}

void LimeSDRInput::suspendTxBuddies()
{
    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("LimeSDRInput::suspendTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

        if ((buddySharedPtr->m_thread) && buddySharedPtr->m_thread->isRunning())
        {
            buddySharedPtr->m_thread->stopWork();
            buddySharedPtr->m_threadWasRunning = true;
        }
        else
        {
            buddySharedPtr->m_threadWasRunning = false;
        }
    }
}

void LimeSDRInput::resumeRxBuddies()
{
    const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
    std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

    qDebug("LimeSDRInput::resumeRxBuddies (%lu)", sourceBuddies.size());

    for (; itSource != sourceBuddies.end(); ++itSource)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSource)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void LimeSDRInput::resumeTxBuddies()
{
    const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
    std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

    qDebug("LimeSDRInput::resumeTxBuddies (%lu)", sinkBuddies.size());

    for (; itSink != sinkBuddies.end(); ++itSink)
    {
        DeviceLimeSDRShared *buddySharedPtr = (DeviceLimeSDRShared *) (*itSink)->getBuddySharedPtr();

        if (buddySharedPtr->m_threadWasRunning) {
            buddySharedPtr->m_thread->startWork();
        }
    }
}

void LimeSDRInput::closeDevice()
{
    if (m_deviceShared.m_deviceParams->getDevice() == 0) { // was never open
        return;
    }

    if (m_running) { stop(); }

    m_deviceShared.m_channel = -1;

    // No buddies so effectively close the device

    if ((m_deviceAPI->getSinkBuddies().size() == 0) && (m_deviceAPI->getSourceBuddies().size() == 0))
    {
        m_deviceShared.m_deviceParams->close();
        delete m_deviceShared.m_deviceParams;
        m_deviceShared.m_deviceParams = 0;
    }
}

bool LimeSDRInput::acquireChannel()
{
    suspendRxBuddies();
    suspendTxBuddies();

    // acquire the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_RX, m_deviceShared.m_channel, true) != 0)
    {
        qCritical("LimeSDRInput::acquireChannel: cannot enable Rx channel %d", m_deviceShared.m_channel);
        return false;
    }
    else
    {
        qDebug("LimeSDRInput::acquireChannel: Rx channel %d enabled", m_deviceShared.m_channel);
    }

    // set up the stream

    m_streamId.channel =  m_deviceShared.m_channel; // channel number
    m_streamId.fifoSize = 1024 * 1024;              // fifo size in samples (SR / 10 take ~5MS/s)
    m_streamId.throughputVsLatency = 0.5;           // optimize for min latency
    m_streamId.isTx = false;                        // RX channel
    m_streamId.dataFmt = lms_stream_t::LMS_FMT_I12; // 12-bit integers

    if (LMS_SetupStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qCritical("LimeSDRInput::acquireChannel: cannot setup the stream on Rx channel %d", m_deviceShared.m_channel);
        resumeTxBuddies();
        resumeRxBuddies();
        return false;
    }
    else
    {
        qDebug("LimeSDRInput::acquireChannel: stream set up on Rx channel %d", m_deviceShared.m_channel);
    }

    resumeTxBuddies();
    resumeRxBuddies();

    m_channelAcquired = true;

    return true;
}

void LimeSDRInput::releaseChannel()
{
    suspendRxBuddies();
    suspendTxBuddies();

    // destroy the stream
    if (LMS_DestroyStream(m_deviceShared.m_deviceParams->getDevice(), &m_streamId) != 0)
    {
        qWarning("LimeSDRInput::releaseChannel: cannot destroy the stream on Rx channel %d", m_deviceShared.m_channel);
    }
    else
    {
        qDebug("LimeSDRInput::releaseChannel: stream destroyed on Rx channel %d", m_deviceShared.m_channel);
    }

    m_streamId.handle = 0;

    // release the channel

    if (LMS_EnableChannel(m_deviceShared.m_deviceParams->getDevice(), LMS_CH_RX, m_deviceShared.m_channel, false) != 0)
    {
        qWarning("LimeSDRInput::releaseChannel: cannot disable Rx channel %d", m_deviceShared.m_channel);
    }
    else
    {
        qDebug("LimeSDRInput::releaseChannel: Rx channel %d disabled", m_deviceShared.m_channel);
    }

    resumeTxBuddies();
    resumeRxBuddies();

    // The channel will be effectively released to be reused in another device set only at close time

    m_channelAcquired = false;
}

void LimeSDRInput::init()
{
    applySettings(m_settings, true, false);
}

bool LimeSDRInput::start()
{
    if (!m_deviceShared.m_deviceParams->getDevice()) {
        return false;
    }

    if (m_running) { stop(); }

    if (!acquireChannel())
    {
        return false;
    }

    // start / stop streaming is done in the thread.

    m_limeSDRInputThread = new LimeSDRInputThread(&m_streamId, &m_sampleFifo);
    qDebug("LimeSDRInput::start: thread created");

    applySettings(m_settings, true);

    m_limeSDRInputThread->setLog2Decimation(m_settings.m_log2SoftDecim);

    m_limeSDRInputThread->startWork();

    m_deviceShared.m_thread = m_limeSDRInputThread;
    m_running = true;

    return true;
}

void LimeSDRInput::stop()
{
    qDebug("LimeSDRInput::stop");

    if (m_limeSDRInputThread != 0)
    {
        m_limeSDRInputThread->stopWork();
        delete m_limeSDRInputThread;
        m_limeSDRInputThread = 0;
    }

    m_deviceShared.m_thread = 0;
    m_running = false;

    releaseChannel();
}

QByteArray LimeSDRInput::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDRInput::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureLimeSDR* message = MsgConfigureLimeSDR::create(m_settings, true);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDR* messageToGUI = MsgConfigureLimeSDR::create(m_settings, true);
        m_guiMessageQueue->push(messageToGUI);
    }

    return success;
}

const QString& LimeSDRInput::getDeviceDescription() const
{
    return m_deviceDescription;
}

int LimeSDRInput::getSampleRate() const
{
    int rate = m_settings.m_devSampleRate;
    return (rate / (1<<m_settings.m_log2SoftDecim));
}

quint64 LimeSDRInput::getCenterFrequency() const
{
    return m_settings.m_centerFrequency + (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);
}

void LimeSDRInput::setCenterFrequency(qint64 centerFrequency)
{
    LimeSDRInputSettings settings = m_settings;
    settings.m_centerFrequency = centerFrequency - (m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0);

    MsgConfigureLimeSDR* message = MsgConfigureLimeSDR::create(settings, false);
    m_inputMessageQueue.push(message);

    if (m_guiMessageQueue)
    {
        MsgConfigureLimeSDR* messageToGUI = MsgConfigureLimeSDR::create(settings, false);
        m_guiMessageQueue->push(messageToGUI);
    }
}

std::size_t LimeSDRInput::getChannelIndex()
{
    return m_deviceShared.m_channel;
}

void LimeSDRInput::getLORange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_loRangeRx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDRInput::getLORange: min: %f max: %f", range.min, range.max);
}

void LimeSDRInput::getSRRange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_srRangeRx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDRInput::getSRRange: min: %f max: %f", range.min, range.max);
}

void LimeSDRInput::getLPRange(float& minF, float& maxF) const
{
    lms_range_t range = m_deviceShared.m_deviceParams->m_lpfRangeRx;
    minF = range.min;
    maxF = range.max;
    qDebug("LimeSDRInput::getLPRange: min: %f max: %f", range.min, range.max);
}

uint32_t LimeSDRInput::getHWLog2Decim() const
{
    return m_deviceShared.m_deviceParams->m_log2OvSRRx;
}

bool LimeSDRInput::handleMessage(const Message& message)
{
    if (MsgConfigureLimeSDR::match(message))
    {
        MsgConfigureLimeSDR& conf = (MsgConfigureLimeSDR&) message;
        qDebug() << "LimeSDRInput::handleMessage: MsgConfigureLimeSDR";

        if (!applySettings(conf.getSettings(), conf.getForce()))
        {
            qDebug("LimeSDRInput::handleMessage config error");
        }

        return true;
    }
    else if (DeviceLimeSDRShared::MsgReportBuddyChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportBuddyChange& report = (DeviceLimeSDRShared::MsgReportBuddyChange&) message;

        if (report.getRxElseTx())
        {
            m_settings.m_devSampleRate   = report.getDevSampleRate();
            m_settings.m_log2HardDecim   = report.getLog2HardDecimInterp();
            m_settings.m_centerFrequency = report.getCenterFrequency();
        }
        else if (m_running)
        {
            double host_Hz;
            double rf_Hz;

            if (LMS_GetSampleRate(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    &host_Hz,
                    &rf_Hz) < 0)
            {
                qDebug("LimeSDRInput::handleMessage: MsgReportBuddyChange: LMS_GetSampleRate() failed");
            }
            else
            {
                m_settings.m_devSampleRate = roundf(host_Hz);
                int hard = roundf(rf_Hz) / m_settings.m_devSampleRate;
                m_settings.m_log2HardDecim = log2(hard);

                qDebug() << "LimeSDRInput::handleMessage: MsgReportBuddyChange:"
                         << " host_Hz: " << host_Hz
                         << " rf_Hz: " << rf_Hz
                         << " m_devSampleRate: " << m_settings.m_devSampleRate
                         << " log2Hard: " << hard
                         << " m_log2HardDecim: " << m_settings.m_log2HardDecim;
//                int adcdac_rate = report.getDevSampleRate() * (1<<report.getLog2HardDecimInterp());
//                m_settings.m_devSampleRate = adcdac_rate / (1<<m_settings.m_log2HardDecim); // new device to host sample rate
            }
        }

        if (m_settings.m_ncoEnable) // need to reset NCO after sample rate change
        {
            applySettings(m_settings, false, true);
        }

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        DSPSignalNotification *notif = new DSPSignalNotification(
                m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim),
                m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        DeviceLimeSDRShared::MsgReportBuddyChange *reportToGUI = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
        getMessageQueueToGUI()->push(reportToGUI);

        return true;
    }
    else if (DeviceLimeSDRShared::MsgReportClockSourceChange::match(message))
    {
        DeviceLimeSDRShared::MsgReportClockSourceChange& report = (DeviceLimeSDRShared::MsgReportClockSourceChange&) message;

        m_settings.m_extClock     = report.getExtClock();
        m_settings.m_extClockFreq = report.getExtClockFeq();

        DeviceLimeSDRShared::MsgReportClockSourceChange *reportToGUI = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                m_settings.m_extClock, m_settings.m_extClockFreq);
        getMessageQueueToGUI()->push(reportToGUI);

        return true;
    }
    else if (MsgGetStreamInfo::match(message))
    {
//        qDebug() << "LimeSDRInput::handleMessage: MsgGetStreamInfo";
        lms_stream_status_t status;

        if (m_streamId.handle && (LMS_GetStreamStatus(&m_streamId, &status) == 0))
        {
            if (m_deviceAPI->getSampleSourceGUIMessageQueue())
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        true, // Success
                        status.active,
                        status.fifoFilledCount,
                        status.fifoSize,
                        status.underrun,
                        status.overrun,
                        status.droppedPackets,
                        status.linkRate,
                        status.timestamp);
                m_deviceAPI->getSampleSourceGUIMessageQueue()->push(report);
            }
        }
        else
        {
            if (m_deviceAPI->getSampleSourceGUIMessageQueue())
            {
                MsgReportStreamInfo *report = MsgReportStreamInfo::create(
                        false, // Success
                        false, // status.active,
                        0,     // status.fifoFilledCount,
                        16384, // status.fifoSize,
                        0,     // status.underrun,
                        0,     // status.overrun,
                        0,     // status.droppedPackets,
                        0,     // status.linkRate,
                        0);    // status.timestamp);
                m_deviceAPI->getSampleSourceGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgGetDeviceInfo::match(message))
    {
        double temp = 0.0;

        if (m_deviceShared.m_deviceParams->getDevice() && (LMS_GetChipTemperature(m_deviceShared.m_deviceParams->getDevice(), 0, &temp) == 0))
        {
            //qDebug("LimeSDRInput::handleMessage: MsgGetDeviceInfo: temperature: %f", temp);
        }
        else
        {
            qDebug("LimeSDRInput::handleMessage: MsgGetDeviceInfo: cannot get temperature");
        }

        // send to oneself
        if (m_deviceAPI->getSampleSourceGUIMessageQueue()) {
            DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp);
            m_deviceAPI->getSampleSourceGUIMessageQueue()->push(report);
        }

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            if ((*itSource)->getSampleSourceGUIMessageQueue())
            {
                DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp);
                (*itSource)->getSampleSourceGUIMessageQueue()->push(report);
            }
        }

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            if ((*itSink)->getSampleSinkGUIMessageQueue())
            {
                DeviceLimeSDRShared::MsgReportDeviceInfo *report = DeviceLimeSDRShared::MsgReportDeviceInfo::create(temp);
                (*itSink)->getSampleSinkGUIMessageQueue()->push(report);
            }
        }

        return true;
    }
    else if (MsgFileRecord::match(message))
    {
        MsgFileRecord& conf = (MsgFileRecord&) message;
        qDebug() << "LimeSDRInput::handleMessage: MsgFileRecord: " << conf.getStartStop();

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
    else if (MsgStartStop::match(message))
    {
        MsgStartStop& cmd = (MsgStartStop&) message;
        qDebug() << "LimeSDRInput::handleMessage: MsgStartStop: " << (cmd.getStartStop() ? "start" : "stop");

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
    else
    {
        return false;
    }
}

bool LimeSDRInput::applySettings(const LimeSDRInputSettings& settings, bool force, bool forceNCOFrequency)
{
    bool forwardChangeOwnDSP = false;
    bool forwardChangeRxDSP  = false;
    bool forwardChangeAllDSP = false;
    bool forwardClockSource  = false;
    bool ownThreadWasRunning = false;
    bool doCalibration = false;
    bool doLPCalibration = false;
    bool setAntennaAuto = false;
    double clockGenFreq      = 0.0;
//  QMutexLocker mutexLocker(&m_mutex);

    qint64 deviceCenterFrequency = settings.m_centerFrequency;
    deviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;

    if (LMS_GetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreq) != 0)
    {
        qCritical("LimeSDRInput::applySettings: could not get clock gen frequency");
    }
    else
    {
        qDebug() << "LimeSDRInput::applySettings: clock gen frequency: " << clockGenFreq;
    }

    // apply settings

    if ((m_settings.m_dcBlock != settings.m_dcBlock) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_iqCorrection != settings.m_iqCorrection) || force)
    {
        m_deviceAPI->configureCorrections(settings.m_dcBlock, settings.m_iqCorrection);
    }

    if ((m_settings.m_gainMode != settings.m_gainMode) || force)
    {
        if (settings.m_gainMode == LimeSDRInputSettings::GAIN_AUTO)
        {
            if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
            {
                if (LMS_SetGaindB(m_deviceShared.m_deviceParams->getDevice(),
                        LMS_CH_RX,
                        m_deviceShared.m_channel,
                        settings.m_gain) < 0)
                {
                    qDebug("LimeSDRInput::applySettings: LMS_SetGaindB() failed");
                }
                else
                {
                    doCalibration = true;
                    qDebug() << "LimeSDRInput::applySettings: Gain (auto) set to " << settings.m_gain;
                }
            }
        }
        else
        {
            if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
            {
                if (DeviceLimeSDR::SetRFELNA_dB(m_deviceShared.m_deviceParams->getDevice(),
                        m_deviceShared.m_channel,
                        settings.m_lnaGain))
                {
                    doCalibration = true;
                    qDebug() << "LimeSDRInput::applySettings: LNA gain (manual) set to " << settings.m_lnaGain;
                }
                else
                {
                    qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRFELNA_dB() failed");
                }

                if (DeviceLimeSDR::SetRFETIA_dB(m_deviceShared.m_deviceParams->getDevice(),
                        m_deviceShared.m_channel,
                        settings.m_tiaGain))
                {
                    doCalibration = true;
                    qDebug() << "LimeSDRInput::applySettings: TIA gain (manual) set to " << settings.m_tiaGain;
                }
                else
                {
                    qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRFETIA_dB() failed");
                }

                if (DeviceLimeSDR::SetRBBPGA_dB(m_deviceShared.m_deviceParams->getDevice(),
                        m_deviceShared.m_channel,
                        settings.m_pgaGain))
                {
                    doCalibration = true;
                    qDebug() << "LimeSDRInput::applySettings: PGA gain (manual) set to " << settings.m_pgaGain;
                }
                else
                {
                    qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRBBPGA_dB() failed");
                }
            }
        }
    }

    if ((m_settings.m_gainMode == LimeSDRInputSettings::GAIN_AUTO) && (m_settings.m_gain != settings.m_gain))
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (LMS_SetGaindB(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    settings.m_gain) < 0)
            {
                qDebug("LimeSDRInput::applySettings: LMS_SetGaindB() failed");
            }
            else
            {
                doCalibration = true;
                qDebug() << "LimeSDRInput::applySettings: Gain (auto) set to " << settings.m_gain;
            }
        }
    }

    if ((m_settings.m_gainMode == LimeSDRInputSettings::GAIN_MANUAL) && (m_settings.m_lnaGain != settings.m_lnaGain))
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (DeviceLimeSDR::SetRFELNA_dB(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_lnaGain))
            {
                doCalibration = true;
                qDebug() << "LimeSDRInput::applySettings: LNA gain (manual) set to " << settings.m_lnaGain;
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRFELNA_dB() failed");
            }
        }
    }

    if ((m_settings.m_gainMode == LimeSDRInputSettings::GAIN_MANUAL) && (m_settings.m_tiaGain != settings.m_tiaGain))
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (DeviceLimeSDR::SetRFETIA_dB(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_tiaGain))
            {
                doCalibration = true;
                qDebug() << "LimeSDRInput::applySettings: TIA gain (manual) set to " << settings.m_tiaGain;
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRFETIA_dB() failed");
            }
        }
    }

    if ((m_settings.m_gainMode == LimeSDRInputSettings::GAIN_MANUAL) && (m_settings.m_pgaGain != settings.m_pgaGain))
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (DeviceLimeSDR::SetRBBPGA_dB(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_pgaGain))
            {
                doCalibration = true;
                qDebug() << "LimeSDRInput::applySettings: PGA gain (manual) set to " << settings.m_pgaGain;
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: DeviceLimeSDR::SetRBBPGA_dB() failed");
            }
        }
    }

    if ((m_settings.m_devSampleRate != settings.m_devSampleRate)
       || (m_settings.m_log2HardDecim != settings.m_log2HardDecim) || force)
    {
        forwardChangeAllDSP = true; //m_settings.m_devSampleRate != settings.m_devSampleRate;

        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (LMS_SetSampleRateDir(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    settings.m_devSampleRate,
                    1<<settings.m_log2HardDecim) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set sample rate to %d with oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardDecim);
            }
            else
            {
                m_deviceShared.m_deviceParams->m_log2OvSRRx = settings.m_log2HardDecim;
                m_deviceShared.m_deviceParams->m_sampleRate = settings.m_devSampleRate;
                //doCalibration = true;
                forceNCOFrequency = true;
                qDebug("LimeSDRInput::applySettings: set sample rate set to %d with oversampling of %d",
                        settings.m_devSampleRate,
                        1<<settings.m_log2HardDecim);
            }
        }
    }

    if ((m_settings.m_lpfBW != settings.m_lpfBW) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            doLPCalibration = true;
        }
    }

    if ((m_settings.m_lpfFIRBW != settings.m_lpfFIRBW) ||
        (m_settings.m_lpfFIREnable != settings.m_lpfFIREnable) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (LMS_SetGFIRLPF(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    settings.m_lpfFIREnable,
                    settings.m_lpfFIRBW) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could %s and set LPF FIR to %f Hz",
                        settings.m_lpfFIREnable ? "enable" : "disable",
                        settings.m_lpfFIRBW);
            }
            else
            {
                //doCalibration = true;
                qDebug("LimeSDRInput::applySettings: %sd and set LPF FIR to %f Hz",
                        settings.m_lpfFIREnable ? "enable" : "disable",
                        settings.m_lpfFIRBW);
            }
        }
    }

    if ((m_settings.m_ncoFrequency != settings.m_ncoFrequency) ||
        (m_settings.m_ncoEnable != settings.m_ncoEnable) || force || forceNCOFrequency)
    {
        forwardChangeOwnDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (DeviceLimeSDR::setNCOFrequency(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    settings.m_ncoEnable,
                    settings.m_ncoFrequency))
            {
                //doCalibration = true;
                m_deviceShared.m_ncoFrequency = settings.m_ncoEnable ? settings.m_ncoFrequency : 0; // for buddies
                qDebug("LimeSDRInput::applySettings: %sd and set NCO to %d Hz",
                        settings.m_ncoEnable ? "enable" : "disable",
                        settings.m_ncoFrequency);
            }
            else
            {
                qCritical("LimeSDRInput::applySettings: could not %s and set NCO to %d Hz",
                        settings.m_ncoEnable ? "enable" : "disable",
                        settings.m_ncoFrequency);
            }
        }
    }

    if ((m_settings.m_log2SoftDecim != settings.m_log2SoftDecim) || force)
    {
        forwardChangeOwnDSP = true;
        m_deviceShared.m_log2Soft = settings.m_log2SoftDecim; // for buddies

        if (m_limeSDRInputThread != 0)
        {
            m_limeSDRInputThread->setLog2Decimation(settings.m_log2SoftDecim);
            qDebug() << "LimeSDRInput::applySettings: set soft decimation to " << (1<<settings.m_log2SoftDecim);
        }
    }

    if ((m_settings.m_antennaPath != settings.m_antennaPath) || force)
    {
        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (DeviceLimeSDR::setRxAntennaPath(m_deviceShared.m_deviceParams->getDevice(),
                    m_deviceShared.m_channel,
                    settings.m_antennaPath))
            {
                doCalibration = true;
                //setAntennaAuto = (settings.m_antennaPath == 0);
                qDebug("LimeSDRInput::applySettings: set antenna path to %d on channel %d",
                        (int) settings.m_antennaPath,
                        m_deviceShared.m_channel);
            }
            else
            {
                qCritical("LimeSDRInput::applySettings: could not set antenna path to %d",
                        (int) settings.m_antennaPath);
            }
        }
    }

    if ((m_settings.m_centerFrequency != settings.m_centerFrequency)
        || (m_settings.m_transverterMode != settings.m_transverterMode)
        || (m_settings.m_transverterDeltaFrequency != settings.m_transverterDeltaFrequency)
        || setAntennaAuto || force)
    {
        forwardChangeRxDSP = true;

        if (m_deviceShared.m_deviceParams->getDevice() != 0 && m_channelAcquired)
        {
            if (LMS_SetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_SXR, deviceCenterFrequency) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set frequency to %lld", deviceCenterFrequency);
            }
            else
            {
                doCalibration = true;
                m_deviceShared.m_centerFrequency = deviceCenterFrequency; // for buddies
                qDebug("LimeSDRInput::applySettings: frequency set to %lld", deviceCenterFrequency);
            }
        }
    }

    if ((m_settings.m_extClock != settings.m_extClock) ||
        (settings.m_extClock && (m_settings.m_extClockFreq != settings.m_extClockFreq)) || force)
    {

        if (DeviceLimeSDR::setClockSource(m_deviceShared.m_deviceParams->getDevice(),
                settings.m_extClock,
                settings.m_extClockFreq))
        {
            forwardClockSource = true;
            doCalibration = true;
            qDebug("LimeSDRInput::applySettings: clock set to %s (Ext: %d Hz)",
                    settings.m_extClock ? "external" : "internal",
                    settings.m_extClockFreq);
        }
        else
        {
            qCritical("LimeSDRInput::applySettings: could not set clock to %s (Ext: %d Hz)",
                    settings.m_extClock ? "external" : "internal",
                    settings.m_extClockFreq);
        }
    }

    m_settings = settings;
    double clockGenFreqAfter;

    if (LMS_GetClockFreq(m_deviceShared.m_deviceParams->getDevice(), LMS_CLOCK_CGEN, &clockGenFreqAfter) != 0)
    {
        qCritical("LimeSDRInput::applySettings: could not get clock gen frequency");
    }
    else
    {
        qDebug() << "LimeSDRInput::applySettings: clock gen frequency after: " << clockGenFreqAfter;
        doCalibration = doCalibration || (clockGenFreqAfter != clockGenFreq);
    }

    if (doCalibration || doLPCalibration)
    {
        if (m_limeSDRInputThread && m_limeSDRInputThread->isRunning())
        {
            m_limeSDRInputThread->stopWork();
            ownThreadWasRunning = true;
        }

        suspendRxBuddies();
        suspendTxBuddies();

        if (doCalibration)
        {
            if (LMS_Calibrate(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    m_settings.m_devSampleRate,
                    0) < 0)
            {
                qCritical("LimeSDRInput::applySettings: calibration failed on Rx channel %d", m_deviceShared.m_channel);
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: calibration successful on Rx channel %d", m_deviceShared.m_channel);
            }
        }

        if (doLPCalibration)
        {
            if (LMS_SetLPFBW(m_deviceShared.m_deviceParams->getDevice(),
                    LMS_CH_RX,
                    m_deviceShared.m_channel,
                    m_settings.m_lpfBW) < 0)
            {
                qCritical("LimeSDRInput::applySettings: could not set LPF to %f Hz", m_settings.m_lpfBW);
            }
            else
            {
                qDebug("LimeSDRInput::applySettings: LPF set to %f Hz", m_settings.m_lpfBW);
            }
        }

        resumeTxBuddies();
        resumeRxBuddies();

        if (ownThreadWasRunning) {
            m_limeSDRInputThread->startWork();
        }
    }

    // forward changes to buddies or oneself

    if (forwardChangeAllDSP)
    {
        qDebug("LimeSDRInput::applySettings: forward change to all buddies");

        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(
                m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim),
                m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSink)->getSampleSinkInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeRxDSP)
    {
        qDebug("LimeSDRInput::applySettings: forward change to Rx buddies");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;

        // send to self first
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportBuddyChange *report = DeviceLimeSDRShared::MsgReportBuddyChange::create(
                    m_settings.m_devSampleRate, m_settings.m_log2HardDecim, m_settings.m_centerFrequency, true);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
        }
    }
    else if (forwardChangeOwnDSP)
    {
        qDebug("LimeSDRInput::applySettings: forward change to self only");

        int sampleRate = m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim);
        int ncoShift = m_settings.m_ncoEnable ? m_settings.m_ncoFrequency : 0;
        DSPSignalNotification *notif = new DSPSignalNotification(sampleRate, m_settings.m_centerFrequency + ncoShift);
        m_fileSink->handleMessage(*notif); // forward to file sink
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    }

    if (forwardClockSource)
    {
        // send to source buddies
        const std::vector<DeviceSourceAPI*>& sourceBuddies = m_deviceAPI->getSourceBuddies();
        std::vector<DeviceSourceAPI*>::const_iterator itSource = sourceBuddies.begin();

        for (; itSource != sourceBuddies.end(); ++itSource)
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *report = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSource)->getSampleSourceInputMessageQueue()->push(report);
        }

        // send to sink buddies
        const std::vector<DeviceSinkAPI*>& sinkBuddies = m_deviceAPI->getSinkBuddies();
        std::vector<DeviceSinkAPI*>::const_iterator itSink = sinkBuddies.begin();

        for (; itSink != sinkBuddies.end(); ++itSink)
        {
            DeviceLimeSDRShared::MsgReportClockSourceChange *report = DeviceLimeSDRShared::MsgReportClockSourceChange::create(
                    m_settings.m_extClock, m_settings.m_extClockFreq);
            (*itSink)->getSampleSinkInputMessageQueue()->push(report);
        }
    }

    QLocale loc;

    qDebug().noquote() << "LimeSDRInput::applySettings: center freq: " << m_settings.m_centerFrequency << " Hz"
            << " m_transverterMode: " << m_settings.m_transverterMode
            << " m_transverterDeltaFrequency: " << m_settings.m_transverterDeltaFrequency
            << " deviceCenterFrequency: " << deviceCenterFrequency
            << " device stream sample rate: " << loc.toString(m_settings.m_devSampleRate) << "S/s"
            << " sample rate with soft decimation: " << loc.toString( m_settings.m_devSampleRate/(1<<m_settings.m_log2SoftDecim)) << "S/s"
            << " ADC sample rate with hard decimation: " << loc.toString(m_settings.m_devSampleRate*(1<<m_settings.m_log2HardDecim)) << "S/s"
            << " m_log2HardDecim: " << m_settings.m_log2HardDecim
            << " m_log2SoftDecim: " << m_settings.m_log2SoftDecim
            << " m_gain: " << m_settings.m_gain
            << " m_lpfBW: " << loc.toString(static_cast<int>(m_settings.m_lpfBW))
            << " m_lpfFIRBW: " << loc.toString(static_cast<int>(m_settings.m_lpfFIRBW))
            << " m_lpfFIREnable: " << m_settings.m_lpfFIREnable
            << " m_ncoEnable: " << m_settings.m_ncoEnable
            << " m_ncoFrequency: " << loc.toString(m_settings.m_ncoFrequency)
            << " m_antennaPath: " << m_settings.m_antennaPath
            << " m_extClock: " << m_settings.m_extClock
            << " m_extClockFreq: " << loc.toString(m_settings.m_extClockFreq)
            << " force: " << force
            << " forceNCOFrequency: " << forceNCOFrequency
            << " doCalibration: " << doCalibration
            << " doLPCalibration: " << doLPCalibration;

    return true;
}

int LimeSDRInput::webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage __attribute__((unused)))
{
    response.setLimeSdrInputSettings(new SWGSDRangel::SWGLimeSdrInputSettings());
    response.getLimeSdrInputSettings()->init();
    webapiFormatDeviceSettings(response, m_settings);
    return 200;
}

int LimeSDRInput::webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage __attribute__((unused)))
{
    LimeSDRInputSettings settings = m_settings;

    if (deviceSettingsKeys.contains("antennaPath")) {
        settings.m_antennaPath = (LimeSDRInputSettings::PathRFE) response.getLimeSdrInputSettings()->getAntennaPath();
    }
    if (deviceSettingsKeys.contains("centerFrequency")) {
        settings.m_centerFrequency = response.getLimeSdrInputSettings()->getCenterFrequency();
    }
    if (deviceSettingsKeys.contains("dcBlock")) {
        settings.m_dcBlock = response.getLimeSdrInputSettings()->getDcBlock() != 0;
    }
    if (deviceSettingsKeys.contains("devSampleRate")) {
        settings.m_devSampleRate = response.getLimeSdrInputSettings()->getDevSampleRate();
    }
    if (deviceSettingsKeys.contains("extClock")) {
        settings.m_extClock = response.getLimeSdrInputSettings()->getExtClock() != 0;
    }
    if (deviceSettingsKeys.contains("extClockFreq")) {
        settings.m_extClockFreq = response.getLimeSdrInputSettings()->getExtClockFreq();
    }
    if (deviceSettingsKeys.contains("gain")) {
        settings.m_gain = response.getLimeSdrInputSettings()->getGain();
    }
    if (deviceSettingsKeys.contains("gainMode")) {
        settings.m_gainMode = (LimeSDRInputSettings::GainMode) response.getLimeSdrInputSettings()->getGainMode();
    }
    if (deviceSettingsKeys.contains("iqCorrection")) {
        settings.m_iqCorrection = response.getLimeSdrInputSettings()->getIqCorrection() != 0;
    }
    if (deviceSettingsKeys.contains("lnaGain")) {
        settings.m_lnaGain = response.getLimeSdrInputSettings()->getLnaGain();
    }
    if (deviceSettingsKeys.contains("log2HardDecim")) {
        settings.m_log2HardDecim = response.getLimeSdrInputSettings()->getLog2HardDecim();
    }
    if (deviceSettingsKeys.contains("log2SoftDecim")) {
        settings.m_log2SoftDecim = response.getLimeSdrInputSettings()->getLog2SoftDecim();
    }
    if (deviceSettingsKeys.contains("lpfBW")) {
        settings.m_lpfBW = response.getLimeSdrInputSettings()->getLpfBw();
    }
    if (deviceSettingsKeys.contains("lpfFIREnable")) {
        settings.m_lpfFIREnable = response.getLimeSdrInputSettings()->getLpfFirEnable() != 0;
    }
    if (deviceSettingsKeys.contains("lpfFIRBW")) {
        settings.m_lpfFIRBW = response.getLimeSdrInputSettings()->getLpfFirbw();
    }
    if (deviceSettingsKeys.contains("ncoEnable")) {
        settings.m_ncoEnable = response.getLimeSdrInputSettings()->getNcoEnable() != 0;
    }
    if (deviceSettingsKeys.contains("ncoFrequency")) {
        settings.m_ncoFrequency = response.getLimeSdrInputSettings()->getNcoFrequency();
    }
    if (deviceSettingsKeys.contains("pgaGain")) {
        settings.m_pgaGain = response.getLimeSdrInputSettings()->getPgaGain();
    }
    if (deviceSettingsKeys.contains("tiaGain")) {
        settings.m_tiaGain = response.getLimeSdrInputSettings()->getTiaGain();
    }
    if (deviceSettingsKeys.contains("transverterDeltaFrequency")) {
        settings.m_transverterDeltaFrequency = response.getLimeSdrInputSettings()->getTransverterDeltaFrequency();
    }
    if (deviceSettingsKeys.contains("transverterMode")) {
        settings.m_transverterMode = response.getLimeSdrInputSettings()->getTransverterMode() != 0;
    }
    if (deviceSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getLimeSdrInputSettings()->getFileRecordName();
    }

    MsgConfigureLimeSDR *msg = MsgConfigureLimeSDR::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLimeSDR *msgToGUI = MsgConfigureLimeSDR::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatDeviceSettings(response, settings);
    return 200;
}

void LimeSDRInput::webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const LimeSDRInputSettings& settings)
{
    response.getLimeSdrInputSettings()->setAntennaPath((int) settings.m_antennaPath);
    response.getLimeSdrInputSettings()->setCenterFrequency(settings.m_centerFrequency);
    response.getLimeSdrInputSettings()->setDcBlock(settings.m_dcBlock ? 1 : 0);
    response.getLimeSdrInputSettings()->setDevSampleRate(settings.m_devSampleRate);
    response.getLimeSdrInputSettings()->setExtClock(settings.m_extClock ? 1 : 0);
    response.getLimeSdrInputSettings()->setExtClockFreq(settings.m_extClockFreq);
    response.getLimeSdrInputSettings()->setGain(settings.m_gain);
    response.getLimeSdrInputSettings()->setGainMode((int) settings.m_gainMode);
    response.getLimeSdrInputSettings()->setIqCorrection(settings.m_iqCorrection ? 1 : 0);
    response.getLimeSdrInputSettings()->setLnaGain(settings.m_lnaGain);
    response.getLimeSdrInputSettings()->setLog2HardDecim(settings.m_log2HardDecim);
    response.getLimeSdrInputSettings()->setLog2SoftDecim(settings.m_log2SoftDecim);
    response.getLimeSdrInputSettings()->setLpfBw(settings.m_lpfBW);
    response.getLimeSdrInputSettings()->setLpfFirEnable(settings.m_lpfFIREnable ? 1 : 0);
    response.getLimeSdrInputSettings()->setLpfFirbw(settings.m_lpfFIRBW);
    response.getLimeSdrInputSettings()->setNcoEnable(settings.m_ncoEnable ? 1 : 0);
    response.getLimeSdrInputSettings()->setNcoFrequency(settings.m_ncoFrequency);
    response.getLimeSdrInputSettings()->setPgaGain(settings.m_pgaGain);
    response.getLimeSdrInputSettings()->setTiaGain(settings.m_tiaGain);
    response.getLimeSdrInputSettings()->setTransverterDeltaFrequency(settings.m_transverterDeltaFrequency);
    response.getLimeSdrInputSettings()->setTransverterMode(settings.m_transverterMode ? 1 : 0);

    if (response.getLimeSdrInputSettings()->getFileRecordName()) {
        *response.getLimeSdrInputSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getLimeSdrInputSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }
}

int LimeSDRInput::webapiReportGet(
        SWGSDRangel::SWGDeviceReport& response,
        QString& errorMessage __attribute__((unused)))
{
    response.setLimeSdrInputReport(new SWGSDRangel::SWGLimeSdrInputReport());
    response.getLimeSdrInputReport()->init();
    webapiFormatDeviceReport(response);
    return 200;
}

int LimeSDRInput::webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage __attribute__((unused)))
{
    m_deviceAPI->getDeviceEngineStateStr(*response.getState());
    return 200;
}

int LimeSDRInput::webapiRun(
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

void LimeSDRInput::webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response)
{
    bool success = false;
    double temp = 0.0;
    lms_stream_status_t status;
    status.active = false;
    status.fifoFilledCount = 0;
    status.fifoSize = 1;
    status.underrun = 0;
    status.overrun = 0;
    status.droppedPackets = 0;
    status.linkRate = 0.0;
    status.timestamp = 0;

    success = (m_streamId.handle && (LMS_GetStreamStatus(&m_streamId, &status) == 0));

    response.getLimeSdrInputReport()->setSuccess(success ? 1 : 0);
    response.getLimeSdrInputReport()->setStreamActive(status.active ? 1 : 0);
    response.getLimeSdrInputReport()->setFifoSize(status.fifoSize);
    response.getLimeSdrInputReport()->setFifoFill(status.fifoFilledCount);
    response.getLimeSdrInputReport()->setUnderrunCount(status.underrun);
    response.getLimeSdrInputReport()->setOverrunCount(status.overrun);
    response.getLimeSdrInputReport()->setDroppedPacketsCount(status.droppedPackets);
    response.getLimeSdrInputReport()->setLinkRate(status.linkRate);
    response.getLimeSdrInputReport()->setHwTimestamp(status.timestamp);

    if (m_deviceShared.m_deviceParams->getDevice()) {
        LMS_GetChipTemperature(m_deviceShared.m_deviceParams->getDevice(), 0, &temp);
    }

    response.getLimeSdrInputReport()->setTemperature(temp);
}
