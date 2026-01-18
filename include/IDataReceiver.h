#ifndef _IDATARECEIVER_H_
#define _IDATARECEIVER_H_

#include "pi_common.h"

#include "wx/wxprec.h"
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif
#define LOG(str)    std::cout << str << std::endl << std::flush;

// #define DEFAULT_FREQUENCY  150         // means 150kHz
#define MODE_OFF    0
#define MODE_SONAR  1
#define MODE_DEPTH  2

#define NUM_SAMPLES 600 // number of samples to expect from the device
#define HEADER_SIZE 6  // size of the message header
#define MAX_DEPTH   10 // meter

PLUGIN_BEGIN_NAMESPACE

/**
 * Interface for defining data receiver (e.g. IP or serial)
*/
class IDataReceiver: public virtual wxThread {

    public:
        virtual void Startup() = 0;
        virtual void Shutdown() = 0;
        virtual uint8_t* GetData() = 0;
        // virtual void SetFrequency(wxUint8 frequency) = 0;
};

PLUGIN_END_NAMESPACE

#endif