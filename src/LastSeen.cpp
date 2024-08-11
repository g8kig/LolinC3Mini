#include "LastSeen.h"
#include <SafeString.h>
/*
   LastSeen.cpp
   by Matthew Ford,  2021/12/06
   (c)2021 Forward Computing and Control Pty. Ltd.
   NSW, Australia  www.forward.com.au
   This code may be freely used for both private and commerical use.
   Provide this copyright is maintained.

*/


LastSeen::LastSeen(const char* name) {
  deviceName[0] = '\0'; // memory not initialized by new
  advertisedName[0] = '\0';
  lastTimeScanned = 0; // not seen yet
  cSFA(sfDeviceName, deviceName);
  sfDeviceName = name;
}

const char* LastSeen::getDeviceName() {
  return (const char*)deviceName;
}

const char* LastSeen::getAdvertisedName() {
  // full advert data
  return (const char*)advertisedName;
}

void LastSeen::setAdvertisedName(const char* advName) {
  cSFA(sfAdvertisedName, advertisedName);
  sfAdvertisedName = advName;
}

void LastSeen::updateLastSeen(unsigned long t) {
  lastTimeScanned = t;
}

unsigned long LastSeen::getLastSeen() {
  return lastTimeScanned;
}
