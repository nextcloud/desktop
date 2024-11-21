#include "datacollectionwrapper.h"

#ifdef BUILDTYPE_RELWITHDEBINFO
    const QString GA_MEASUREMENT_ID = "G-P9KD4TLW0V";  // Verwende diesen String nur wenn wir in Debug bauen
#else
    const QString GA_MEASUREMENT_ID = "G-270CYZ49V0";  // Verwende diesen String nur wenn wir in Release bauen
#endif


DataCollectionWrapper::DataCollectionWrapper(QObject *parent) : QObject(parent) {
}

DataCollectionWrapper::~DataCollectionWrapper() {
}

void DataCollectionWrapper::clicked(const TrackingPage trackingPage, const TrackingElement trackingButton){
    trackEvent( _trackingPageString[trackingPage], _trackingElementString[trackingButton]);
}

void DataCollectionWrapper::opened(const TrackingPage trackingPage){
    trackEvent(_trackingEventString[TrackingEvent::Open], _trackingPageString[trackingPage]);
}

void DataCollectionWrapper::login(){
    trackEvent(QString(), _trackingEventString[TrackingEvent::Login]);
}

void DataCollectionWrapper::accountRemoved(){
    trackEventImmediately(QString(), _trackingEventString[TrackingEvent::Logout]);
}
 
void DataCollectionWrapper::trackEvent(QString page, QString element) {
    GAnalytics::getInstance().sendEvent(page, element);
}

void DataCollectionWrapper::trackEventImmediately(QString page, QString element)
{
    GAnalytics::getInstance().sendEventImmediatley(page, element);
}

void DataCollectionWrapper::setClientID(const QString clientId) {
    GAnalytics::getInstance().setClientID(clientId);
}

void DataCollectionWrapper::setSendData(const bool sendData) {
    GAnalytics::getInstance().enable(sendData);
}

void DataCollectionWrapper::setAccount(const OCC::AccountPtr account) {
    GAnalytics::getInstance().setAccount(account);
}

void DataCollectionWrapper::initDataCollection() {
    GAnalytics* ga = &GAnalytics::getInstance();
    ga->setMeasurementId(GA_MEASUREMENT_ID);
    ga->setSendInterval(3000);
    ga->setLogLevel(GAnalytics::Info);
    ga->enableValidation(false);
}