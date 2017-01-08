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

#ifndef INCLUDE_HACKRFOUTPUTTHREAD_H
#define INCLUDE_HACKRFOUTPUTTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <libhackrf/hackrf.h>

#include "dsp/samplesourcefifo.h"
#include "dsp/interpolators.h"

#define HACKRF_BLOCKSIZE (1<<17)

class HackRFOutputThread : public QThread {
	Q_OBJECT

public:
	HackRFOutputThread(hackrf_device* dev, SampleSourceFifo* sampleFifo, QObject* parent = NULL);
	~HackRFOutputThread();

	void startWork();
	void stopWork();
	void setSamplerate(uint32_t samplerate);
	void setLog2Interpolation(unsigned int log2_interp);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	hackrf_device* m_dev;
	qint16 m_buf[2*HACKRF_BLOCKSIZE];
	SampleVector m_convertBuffer;
	SampleSourceFifo* m_sampleFifo;

	int m_samplerate;
	unsigned int m_log2Interp;
	static HackRFOutputThread *m_this;

    Interpolators<qint16, SDR_SAMP_SZ, 12> m_interpolators;

	void run();
	void callback(qint16* buf, qint32 len);
	static int tx_callback(hackrf_transfer* transfer);
};

#endif // INCLUDE_HACKRFOUTPUTTHREAD_H