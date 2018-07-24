///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
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

#ifndef PLUGINS_CHANNELRX_DEMODAM_AMDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODAM_AMDEMODSETTINGS_H_

#include <QByteArray>

class Serializable;

struct AMDemodSettings
{
    enum SyncAMOperation
    {
        SyncAMDSB,
        SyncAMUSB,
        SyncAMLSB
    };

    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_squelch;
    Real m_volume;
    bool m_audioMute;
    bool m_bandpassEnable;
    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    QString m_audioDeviceName;
    bool m_pll;
    SyncAMOperation m_syncAMOperation;

    AMDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELRX_DEMODAM_AMDEMODSETTINGS_H_ */
