///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODAM_AMMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODAM_AMMODSETTINGS_H_

#include <QByteArray>

class Serializable;

struct AMModSettings
{
    typedef enum
    {
        AMModInputNone,
        AMModInputTone,
        AMModInputFile,
        AMModInputAudio,
        AMModInputCWTone
    } AMModInputAF;

    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    float m_modFactor;
    float m_toneFrequency;
    float m_volumeFactor;
    bool m_channelMute;
    bool m_playLoop;
    quint32 m_rgbColor;
    QString m_title;
    AMModInputAF m_modAFInput;
    QString m_audioDeviceName;

    Serializable *m_channelMarker;
    Serializable *m_cwKeyerGUI;

    AMModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};



#endif /* PLUGINS_CHANNELTX_MODAM_AMMODSETTINGS_H_ */
