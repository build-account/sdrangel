///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QGlobalStatic>
#include <QThread>

#include "dsp/dspengine.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"


DSPEngine::DSPEngine() :
    m_deviceSourceEnginesUIDSequence(0),
    m_deviceSinkEnginesUIDSequence(0),
    m_audioInputDeviceIndex(-1),    // default device
    m_audioOutputDeviceIndex(-1)    // default device
{
	m_dvSerialSupport = false;
    m_masterTimer.start(50);
}

DSPEngine::~DSPEngine()
{
    std::vector<DSPDeviceSourceEngine*>::iterator it = m_deviceSourceEngines.begin();

    while (it != m_deviceSourceEngines.end())
    {
        delete *it;
        ++it;
    }
}

Q_GLOBAL_STATIC(DSPEngine, dspEngine)
DSPEngine *DSPEngine::instance()
{
	return dspEngine;
}

DSPDeviceSourceEngine *DSPEngine::addDeviceSourceEngine()
{
    m_deviceSourceEngines.push_back(new DSPDeviceSourceEngine(m_deviceSourceEnginesUIDSequence));
    m_deviceSourceEnginesUIDSequence++;
    return m_deviceSourceEngines.back();
}

void DSPEngine::removeLastDeviceSourceEngine()
{
    if (m_deviceSourceEngines.size() > 0)
    {
        DSPDeviceSourceEngine *lastDeviceEngine = m_deviceSourceEngines.back();
        delete lastDeviceEngine;
        m_deviceSourceEngines.pop_back();
        m_deviceSourceEnginesUIDSequence--;
    }
}

DSPDeviceSinkEngine *DSPEngine::addDeviceSinkEngine()
{
    m_deviceSinkEngines.push_back(new DSPDeviceSinkEngine(m_deviceSinkEnginesUIDSequence));
    m_deviceSinkEnginesUIDSequence++;
    return m_deviceSinkEngines.back();
}

void DSPEngine::removeLastDeviceSinkEngine()
{
    if (m_deviceSinkEngines.size() > 0)
    {
        DSPDeviceSinkEngine *lastDeviceEngine = m_deviceSinkEngines.back();
        delete lastDeviceEngine;
        m_deviceSinkEngines.pop_back();
        m_deviceSinkEnginesUIDSequence--;
    }
}

DSPDeviceSourceEngine *DSPEngine::getDeviceSourceEngineByUID(uint uid)
{
    std::vector<DSPDeviceSourceEngine*>::iterator it = m_deviceSourceEngines.begin();

    while (it != m_deviceSourceEngines.end())
    {
        if ((*it)->getUID() == uid)
        {
            return *it;
        }

        ++it;
    }

    return 0;
}

DSPDeviceSinkEngine *DSPEngine::getDeviceSinkEngineByUID(uint uid)
{
    std::vector<DSPDeviceSinkEngine*>::iterator it = m_deviceSinkEngines.begin();

    while (it != m_deviceSinkEngines.end())
    {
        if ((*it)->getUID() == uid)
        {
            return *it;
        }

        ++it;
    }

    return 0;
}

#ifdef DSD_USE_SERIALDV
void DSPEngine::setDVSerialSupport(bool support)
{
    if (support)
    {
        m_dvSerialSupport = m_dvSerialEngine.scan();
    }
    else
    {
        m_dvSerialEngine.release();
        m_dvSerialSupport = false;
    }
}
#else
void DSPEngine::setDVSerialSupport(bool support __attribute__((unused)))
{}
#endif

bool DSPEngine::hasDVSerialSupport()
{
#ifdef DSD_USE_SERIALDV
    return m_dvSerialSupport;
#else
    return false;
#endif
}

#ifdef DSD_USE_SERIALDV
void DSPEngine::getDVSerialNames(std::vector<std::string>& deviceNames)
{
    m_dvSerialEngine.getDevicesNames(deviceNames);
}
#else
void DSPEngine::getDVSerialNames(std::vector<std::string>& deviceNames __attribute((unused)))
{}
#endif

#ifdef DSD_USE_SERIALDV
void DSPEngine::pushMbeFrame(
        const unsigned char *mbeFrame,
        int mbeRateIndex,
        int mbeVolumeIndex,
        unsigned char channels,
        bool useHP,
        int upsampling,
        AudioFifo *audioFifo)
{
    m_dvSerialEngine.pushMbeFrame(mbeFrame, mbeRateIndex, mbeVolumeIndex, channels, useHP, upsampling, audioFifo);
}
#else
void DSPEngine::pushMbeFrame(
        const unsigned char *mbeFrame __attribute((unused)),
        int mbeRateIndex __attribute((unused)),
        int mbeVolumeIndex __attribute((unused)),
        unsigned char channels __attribute((unused)),
        bool useHP __attribute((unused)),
        int upsampling __attribute((unused)),
        AudioFifo *audioFifo __attribute((unused)))
{}
#endif
