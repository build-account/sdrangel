/**
 * SDRangel
 * This is the web REST/JSON API of SDRangel SDR software. SDRangel is an Open Source Qt5/OpenGL 3.0+ (4.3+ in Windows) GUI and server Software Defined Radio and signal analyzer in software. It supports Airspy, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube     ---   Limitations and specifcities:       * In SDRangel GUI the first Rx device set cannot be deleted. Conversely the server starts with no device sets and its number of device sets can be reduced to zero by as many calls as necessary to /sdrangel/deviceset with DELETE method.   * Preset import and export from/to file is a server only feature.   * Device set focus is a GUI only feature.   * The following channels are not implemented (status 501 is returned): ATV and DATV demodulators, Channel Analyzer NG, LoRa demodulator   * The device settings and report structures contains only the sub-structure corresponding to the device type. The DeviceSettings and DeviceReport structures documented here shows all of them but only one will be or should be present at a time   * The channel settings and report structures contains only the sub-structure corresponding to the channel type. The ChannelSettings and ChannelReport structures documented here shows all of them but only one will be or should be present at a time    --- 
 *
 * OpenAPI spec version: 4.0.0
 * Contact: f4exb06@gmail.com
 *
 * NOTE: This class is auto generated by the swagger code generator program.
 * https://github.com/swagger-api/swagger-codegen.git
 * Do not edit the class manually.
 */


#include "SWGSDRdaemonSinkSettings.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGSDRdaemonSinkSettings::SWGSDRdaemonSinkSettings(QString* json) {
    init();
    this->fromJson(*json);
}

SWGSDRdaemonSinkSettings::SWGSDRdaemonSinkSettings() {
    center_frequency = 0;
    m_center_frequency_isSet = false;
    sample_rate = 0;
    m_sample_rate_isSet = false;
    log2_interp = 0;
    m_log2_interp_isSet = false;
    tx_delay = 0.0f;
    m_tx_delay_isSet = false;
    nb_fec_blocks = 0;
    m_nb_fec_blocks_isSet = false;
    address = nullptr;
    m_address_isSet = false;
    data_port = 0;
    m_data_port_isSet = false;
    control_port = 0;
    m_control_port_isSet = false;
    specific_parameters = nullptr;
    m_specific_parameters_isSet = false;
}

SWGSDRdaemonSinkSettings::~SWGSDRdaemonSinkSettings() {
    this->cleanup();
}

void
SWGSDRdaemonSinkSettings::init() {
    center_frequency = 0;
    m_center_frequency_isSet = false;
    sample_rate = 0;
    m_sample_rate_isSet = false;
    log2_interp = 0;
    m_log2_interp_isSet = false;
    tx_delay = 0.0f;
    m_tx_delay_isSet = false;
    nb_fec_blocks = 0;
    m_nb_fec_blocks_isSet = false;
    address = new QString("");
    m_address_isSet = false;
    data_port = 0;
    m_data_port_isSet = false;
    control_port = 0;
    m_control_port_isSet = false;
    specific_parameters = new QString("");
    m_specific_parameters_isSet = false;
}

void
SWGSDRdaemonSinkSettings::cleanup() {





    if(address != nullptr) { 
        delete address;
    }


    if(specific_parameters != nullptr) { 
        delete specific_parameters;
    }
}

SWGSDRdaemonSinkSettings*
SWGSDRdaemonSinkSettings::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGSDRdaemonSinkSettings::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&center_frequency, pJson["centerFrequency"], "qint32", "");
    
    ::SWGSDRangel::setValue(&sample_rate, pJson["sampleRate"], "qint32", "");
    
    ::SWGSDRangel::setValue(&log2_interp, pJson["log2Interp"], "qint32", "");
    
    ::SWGSDRangel::setValue(&tx_delay, pJson["txDelay"], "float", "");
    
    ::SWGSDRangel::setValue(&nb_fec_blocks, pJson["nbFECBlocks"], "qint32", "");
    
    ::SWGSDRangel::setValue(&address, pJson["address"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&data_port, pJson["dataPort"], "qint32", "");
    
    ::SWGSDRangel::setValue(&control_port, pJson["controlPort"], "qint32", "");
    
    ::SWGSDRangel::setValue(&specific_parameters, pJson["specificParameters"], "QString", "QString");
    
}

QString
SWGSDRdaemonSinkSettings::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGSDRdaemonSinkSettings::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(m_center_frequency_isSet){
        obj->insert("centerFrequency", QJsonValue(center_frequency));
    }
    if(m_sample_rate_isSet){
        obj->insert("sampleRate", QJsonValue(sample_rate));
    }
    if(m_log2_interp_isSet){
        obj->insert("log2Interp", QJsonValue(log2_interp));
    }
    if(m_tx_delay_isSet){
        obj->insert("txDelay", QJsonValue(tx_delay));
    }
    if(m_nb_fec_blocks_isSet){
        obj->insert("nbFECBlocks", QJsonValue(nb_fec_blocks));
    }
    if(address != nullptr && *address != QString("")){
        toJsonValue(QString("address"), address, obj, QString("QString"));
    }
    if(m_data_port_isSet){
        obj->insert("dataPort", QJsonValue(data_port));
    }
    if(m_control_port_isSet){
        obj->insert("controlPort", QJsonValue(control_port));
    }
    if(specific_parameters != nullptr && *specific_parameters != QString("")){
        toJsonValue(QString("specificParameters"), specific_parameters, obj, QString("QString"));
    }

    return obj;
}

qint32
SWGSDRdaemonSinkSettings::getCenterFrequency() {
    return center_frequency;
}
void
SWGSDRdaemonSinkSettings::setCenterFrequency(qint32 center_frequency) {
    this->center_frequency = center_frequency;
    this->m_center_frequency_isSet = true;
}

qint32
SWGSDRdaemonSinkSettings::getSampleRate() {
    return sample_rate;
}
void
SWGSDRdaemonSinkSettings::setSampleRate(qint32 sample_rate) {
    this->sample_rate = sample_rate;
    this->m_sample_rate_isSet = true;
}

qint32
SWGSDRdaemonSinkSettings::getLog2Interp() {
    return log2_interp;
}
void
SWGSDRdaemonSinkSettings::setLog2Interp(qint32 log2_interp) {
    this->log2_interp = log2_interp;
    this->m_log2_interp_isSet = true;
}

float
SWGSDRdaemonSinkSettings::getTxDelay() {
    return tx_delay;
}
void
SWGSDRdaemonSinkSettings::setTxDelay(float tx_delay) {
    this->tx_delay = tx_delay;
    this->m_tx_delay_isSet = true;
}

qint32
SWGSDRdaemonSinkSettings::getNbFecBlocks() {
    return nb_fec_blocks;
}
void
SWGSDRdaemonSinkSettings::setNbFecBlocks(qint32 nb_fec_blocks) {
    this->nb_fec_blocks = nb_fec_blocks;
    this->m_nb_fec_blocks_isSet = true;
}

QString*
SWGSDRdaemonSinkSettings::getAddress() {
    return address;
}
void
SWGSDRdaemonSinkSettings::setAddress(QString* address) {
    this->address = address;
    this->m_address_isSet = true;
}

qint32
SWGSDRdaemonSinkSettings::getDataPort() {
    return data_port;
}
void
SWGSDRdaemonSinkSettings::setDataPort(qint32 data_port) {
    this->data_port = data_port;
    this->m_data_port_isSet = true;
}

qint32
SWGSDRdaemonSinkSettings::getControlPort() {
    return control_port;
}
void
SWGSDRdaemonSinkSettings::setControlPort(qint32 control_port) {
    this->control_port = control_port;
    this->m_control_port_isSet = true;
}

QString*
SWGSDRdaemonSinkSettings::getSpecificParameters() {
    return specific_parameters;
}
void
SWGSDRdaemonSinkSettings::setSpecificParameters(QString* specific_parameters) {
    this->specific_parameters = specific_parameters;
    this->m_specific_parameters_isSet = true;
}


bool
SWGSDRdaemonSinkSettings::isSet(){
    bool isObjectUpdated = false;
    do{
        if(m_center_frequency_isSet){ isObjectUpdated = true; break;}
        if(m_sample_rate_isSet){ isObjectUpdated = true; break;}
        if(m_log2_interp_isSet){ isObjectUpdated = true; break;}
        if(m_tx_delay_isSet){ isObjectUpdated = true; break;}
        if(m_nb_fec_blocks_isSet){ isObjectUpdated = true; break;}
        if(address != nullptr && *address != QString("")){ isObjectUpdated = true; break;}
        if(m_data_port_isSet){ isObjectUpdated = true; break;}
        if(m_control_port_isSet){ isObjectUpdated = true; break;}
        if(specific_parameters != nullptr && *specific_parameters != QString("")){ isObjectUpdated = true; break;}
    }while(false);
    return isObjectUpdated;
}
}

