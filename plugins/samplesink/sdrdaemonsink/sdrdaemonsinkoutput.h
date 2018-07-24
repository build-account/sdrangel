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

#ifndef INCLUDE_SDRDAEMONSINKOUTPUT_H
#define INCLUDE_SDRDAEMONSINKOUTPUT_H

#include <QString>
#include <QTimer>
#include <ctime>
#include <iostream>
#include <fstream>

#include "dsp/devicesamplesink.h"

#include "sdrdaemonsinksettings.h"

class SDRdaemonSinkThread;
class DeviceSinkAPI;

class SDRdaemonSinkOutput : public DeviceSampleSink {
public:
	class MsgConfigureSDRdaemonSink : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const SDRdaemonSinkSettings& getSettings() const { return m_settings; }
		bool getForce() const { return m_force; }

		static MsgConfigureSDRdaemonSink* create(const SDRdaemonSinkSettings& settings, bool force = false)
		{
			return new MsgConfigureSDRdaemonSink(settings, force);
		}

	private:
		SDRdaemonSinkSettings m_settings;
		bool m_force;

		MsgConfigureSDRdaemonSink(const SDRdaemonSinkSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
		{ }
	};

	class MsgConfigureSDRdaemonSinkWork : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		bool isWorking() const { return m_working; }

		static MsgConfigureSDRdaemonSinkWork* create(bool working)
		{
			return new MsgConfigureSDRdaemonSinkWork(working);
		}

	private:
		bool m_working;

		MsgConfigureSDRdaemonSinkWork(bool working) :
			Message(),
			m_working(working)
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

    class MsgConfigureSDRdaemonSinkChunkCorrection : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getChunkCorrection() const { return m_chunkCorrection; }

        static MsgConfigureSDRdaemonSinkChunkCorrection* create(int chunkCorrection)
        {
            return new MsgConfigureSDRdaemonSinkChunkCorrection(chunkCorrection);
        }

    private:
        int m_chunkCorrection;

        MsgConfigureSDRdaemonSinkChunkCorrection(int chunkCorrection) :
            Message(),
            m_chunkCorrection(chunkCorrection)
        { }
    };

	class MsgConfigureSDRdaemonSinkStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:

		static MsgConfigureSDRdaemonSinkStreamTiming* create()
		{
			return new MsgConfigureSDRdaemonSinkStreamTiming();
		}

	private:

		MsgConfigureSDRdaemonSinkStreamTiming() :
			Message()
		{ }
	};

	class MsgReportSDRdaemonSinkStreamTiming : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		std::size_t getSamplesCount() const { return m_samplesCount; }

		static MsgReportSDRdaemonSinkStreamTiming* create(std::size_t samplesCount)
		{
			return new MsgReportSDRdaemonSinkStreamTiming(samplesCount);
		}

	protected:
		std::size_t m_samplesCount;

		MsgReportSDRdaemonSinkStreamTiming(std::size_t samplesCount) :
			Message(),
			m_samplesCount(samplesCount)
		{ }
	};

	SDRdaemonSinkOutput(DeviceSinkAPI *deviceAPI);
	virtual ~SDRdaemonSinkOutput();
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
	std::time_t getStartingTimeStamp() const;

	virtual bool handleMessage(const Message& message);

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            QString& errorMessage);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

private:
    DeviceSinkAPI *m_deviceAPI;
	QMutex m_mutex;
	SDRdaemonSinkSettings m_settings;
	SDRdaemonSinkThread* m_sdrDaemonSinkThread;
	QString m_deviceDescription;
	std::time_t m_startingTimeStamp;
	const QTimer& m_masterTimer;

	void applySettings(const SDRdaemonSinkSettings& settings, bool force = false);
    void webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const SDRdaemonSinkSettings& settings);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
};

#endif // INCLUDE_SDRDAEMONSINKOUTPUT_H
