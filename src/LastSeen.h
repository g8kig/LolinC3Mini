#ifndef LAST_SEEN_H
#define LAST_SEEN_H
/*
   LastSeen.h
   by Matthew Ford,  2021/12/06
   (c)2021 Forward Computing and Control Pty. Ltd.
   NSW, Australia  www.forward.com.au
   This code may be freely used for both private and commerical use.
   Provide this copyright is maintained.

*/

// LastSeen.h

class LastSeen {
  public:
    LastSeen(const char*name);
    void setAdvertisedName(const char* advName);
    void updateLastSeen(unsigned long t);
    unsigned long getLastSeen();
    const char* getDeviceName();
    const char* getAdvertisedName(); // full advert data
  private:
    char deviceName[33]; // max length 32 + null
    char advertisedName[33]; // max length 32 + null
    unsigned long lastTimeScanned; // when was this last seen
};


#endif
