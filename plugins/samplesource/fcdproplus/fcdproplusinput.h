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

#ifndef INCLUDE_FCDINPUT_H
#define INCLUDE_FCDINPUT_H

#include <QString>
#include <QByteArray>
#include <inttypes.h>

#include <dsp/devicesamplesource.h>
#include "fcdproplussettings.h"
#include "fcdhid.h"

struct fcd_buffer {
	void *start;
	std::size_t length;
};

class DeviceSourceAPI;
class FCDProPlusThread;
class FileRecord;

class FCDProPlusInput : public DeviceSampleSource {
public:
	class MsgConfigureFCDProPlus : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const FCDProPlusSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureFCDProPlus* create(const FCDProPlusSettings& settings, bool force)
		{
			return new MsgConfigureFCDProPlus(settings, force);
		}

	private:
		FCDProPlusSettings m_settings;
		bool m_force;

		MsgConfigureFCDProPlus(const FCDProPlusSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
		{ }
	};

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgFileRecord : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgFileRecord* create(bool startStop) {
            return new MsgFileRecord(startStop);
        }

    protected:
        bool m_startStop;

        MsgFileRecord(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

	FCDProPlusInput(DeviceSourceAPI *deviceAPI);
	virtual ~FCDProPlusInput();
	virtual void destroy();

    virtual void init();
	virtual bool start();
	virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
	virtual const QString& getDeviceDescription() const;
	virtual int getSampleRate() const;
	virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

	virtual bool handleMessage(const Message& message);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    void set_center_freq(double freq);
	void set_bias_t(bool on);
	void set_lna_gain(bool on);
	void set_mixer_gain(bool on);
	void set_if_gain(int gain);
	void set_rf_filter(int filterIndex);
	void set_if_filter(int filterIndex);
	void set_lo_ppm();

private:
    bool openDevice();
    void closeDevice();
	void applySettings(const FCDProPlusSettings& settings, bool force);
    void webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const FCDProPlusSettings& settings);

	DeviceSourceAPI *m_deviceAPI;
	hid_device *m_dev;
	QMutex m_mutex;
	FCDProPlusSettings m_settings;
	FCDProPlusThread* m_FCDThread;
	QString m_deviceDescription;
	bool m_running;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
};

#endif // INCLUDE_FCD_H
