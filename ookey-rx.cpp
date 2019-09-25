#include <limits.h>

volatile int x;

namespace ookey
{
namespace rx
{

const int MARK_PIN = 8;
const int startBitWaitCount = 100;
const int timeSamplesCount = 12;

class Decoder : public applicationEvents::EventHandler
{
  unsigned short address;
  unsigned char buffer[2 + 1 + 256 + 4];
  int bitTime;
  int bitCounter;
  int rxEventId;

  unsigned int timeSamples[timeSamplesCount];
  int timeSampleIdx = 0;

  virtual void setTimerInterrupt(int time) = 0;
  virtual void setRfPinInterrupt(bool enabled) = 0;
  virtual void dataReceived(unsigned char *data, int len) = 0;

  void mark()
  {
    target::PORT.DIRSET.setDIRSET(1 << MARK_PIN);
    target::PORT.OUTSET.setOUTSET(1 << MARK_PIN);
    for (volatile int c = 0; c < 10; c++)
      ;
    target::PORT.OUTCLR.setOUTCLR(1 << MARK_PIN);
  }

  void onEvent()
  {
    dataReceived(&buffer[1], buffer[0]);
  }

public:
  void init(unsigned short address)
  {
    this->address = address;
    rxEventId = applicationEvents::createEventId();
    handle(rxEventId);

    listen();
  }

  void listen()
  {
    setTimerInterrupt(0);
    for (int c = 0; c < timeSamplesCount; c++)
    {
      timeSamples[c] = 0xFFFFFFFF + (c & 1);
    }
    setRfPinInterrupt(true);
  }

////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////

  void handleTimerInterrupt(int rfPinValue)
  {
    if (bitCounter == -startBitWaitCount) {
      setTimerInterrupt(bitTime);
    }

    if (bitCounter < 0)
    {
      if (!rfPinValue) {
        bitCounter = 0;
      } else {
        mark();
        bitCounter++;
        if (bitCounter == 0) {
          listen();
        }
      }
    }
    else
    {
      //mark();
      int byteIdx = bitCounter >> 3;
      int bitIdx = bitCounter & 0x07;

      if (
          (byteIdx == 1 && buffer[0] != (address & 0xFF)) ||
          (byteIdx == 2 && buffer[1] != (address >> 8)))
      {
        listen();
      }
      else
      {
        if (byteIdx > 2 && byteIdx == 2 + 1 + buffer[2] + 2)
        {
          int calculatedCrc = 0x55;
          int len = buffer[2];
          for (int c = 0; c < len; c++)
          {
            calculatedCrc += buffer[c + 2 + 1];
          }
          int bufferCrc = buffer[2 + 1 + len] | buffer[2 + 1 + len + 1] << 8;
          if (bufferCrc == calculatedCrc)
          {
            applicationEvents::schedule(rxEventId);
            setTimerInterrupt(0);
          }
          else
          {
            listen();
          }
        }
        else
        {
          if (bitIdx == 0)
          {
            buffer[byteIdx] = 0;
          }
          buffer[byteIdx] |= (rfPinValue ^ (bitCounter & 1)) << bitIdx;

          bitCounter++;
        }
      }
    }    
  }

  void handleRfPinInterrupt(unsigned int time)
  {
    timeSamples[timeSampleIdx] = time;

    timeSampleIdx++;
    if (timeSampleIdx == timeSamplesCount) {
      timeSampleIdx = 0;
    }

    unsigned int min = 0xFFFFFFFF;
    unsigned int max = 0;
    for (int c = 0; c < timeSamplesCount; c++)
    {
      int v = timeSamples[c];
      if (v > max) max = v;
      if (v < min) min = v;
    }

    bool locked = min > (max - (max >> 4));

    if (locked)
    {
      setRfPinInterrupt(false);
      bitTime = (max + min) / 2;
      bitCounter = -startBitWaitCount;
      mark();
      setTimerInterrupt(bitTime + bitTime / 3);
    } else {
       setTimerInterrupt(0);
    }
  }
};

} // namespace rx
} // namespace ookey