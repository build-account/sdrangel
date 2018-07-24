/**
 * SDRangel
 * This is the web REST/JSON API of SDRangel SDR software. SDRangel is an Open Source Qt5/OpenGL 3.0+ (4.3+ in Windows) GUI and server Software Defined Radio and signal analyzer in software. It supports Airspy, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube     ---   Limitations and specifcities:       * In SDRangel GUI the first Rx device set cannot be deleted. Conversely the server starts with no device sets and its number of device sets can be reduced to zero by as many calls as necessary to /sdrangel/deviceset with DELETE method.   * Stopping instance i.e. /sdrangel with DELETE method is a server only feature. It allows stopping the instance nicely.   * Preset import and export from/to file is a server only feature.   * Device set focus is a GUI only feature.   * The following channels are not implemented (status 501 is returned): ATV demodulator, Channel Analyzer, Channel Analyzer NG, LoRa demodulator, TCP source   * The content type returned is always application/json except in the following cases:     * An incorrect URL was specified: this document is returned as text/html with a status 400    --- 
 *
 * OpenAPI spec version: 4.0.0
 * Contact: f4exb06@gmail.com
 *
 * NOTE: This class is auto generated by the swagger code generator program.
 * https://github.com/swagger-api/swagger-codegen.git
 * Do not edit the class manually.
 */


#include "SWGRtlSdrReport_gains.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGRtlSdrReport_gains::SWGRtlSdrReport_gains(QString* json) {
    init();
    this->fromJson(*json);
}

SWGRtlSdrReport_gains::SWGRtlSdrReport_gains() {
    gain = 0;
    m_gain_isSet = false;
}

SWGRtlSdrReport_gains::~SWGRtlSdrReport_gains() {
    this->cleanup();
}

void
SWGRtlSdrReport_gains::init() {
    gain = 0;
    m_gain_isSet = false;
}

void
SWGRtlSdrReport_gains::cleanup() {

}

SWGRtlSdrReport_gains*
SWGRtlSdrReport_gains::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGRtlSdrReport_gains::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&gain, pJson["gain"], "qint32", "");
    
}

QString
SWGRtlSdrReport_gains::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGRtlSdrReport_gains::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(m_gain_isSet){
        obj->insert("gain", QJsonValue(gain));
    }

    return obj;
}

qint32
SWGRtlSdrReport_gains::getGain() {
    return gain;
}
void
SWGRtlSdrReport_gains::setGain(qint32 gain) {
    this->gain = gain;
    this->m_gain_isSet = true;
}


bool
SWGRtlSdrReport_gains::isSet(){
    bool isObjectUpdated = false;
    do{
        if(m_gain_isSet){ isObjectUpdated = true; break;}
    }while(false);
    return isObjectUpdated;
}
}

