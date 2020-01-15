namespace mark {

int pin;

void init(int gpioPin) {
  pin = gpioPin;
  target::PORT.DIRSET.setDIRSET(1 << pin);
}

void now(int length = 1) {
  target::PORT.OUTSET.setOUTSET(1 << pin);
  int wait = length * 8;
  for (volatile int w = 0; w < wait; w++)
    ;
  target::PORT.OUTCLR.setOUTCLR(1 << pin);
}

} // namespace mark

namespace ookey {
namespace rx {

class Decoder : public applicationEvents::EventHandler {
  unsigned short address;
  unsigned char buffer[2 + 1 + 256 + 2];

  const int MIN_PREAMBLE_BITS = 20; // there is 31 ones sent by transmitter,
                                    // let's be tollerant to noisy start

  int dataBits = -1;
  int preambleBits = 0;
  bool pendingData = false;
  int rxEventId;

  // virtual void setTimerInterrupt(int time) = 0;
  // virtual void setRfPinInterrupt(bool enabled) = 0;
  virtual void dataReceived(unsigned char *data, int len) = 0;

  void onEvent() {
    dataReceived(&buffer[1], buffer[0]);
    restart();
  }

public:
  
  void init(unsigned short address) {
    this->address = address;
    rxEventId = applicationEvents::createEventId();
    handle(rxEventId);
  }

  void restart() {
    dataBits = -1;
    preambleBits = 0;
    pendingData = false;
  }

  void pushBit(int bit) {

    if (dataBits < 0) {

      // preamble
      if (bit) {
        preambleBits++;
      } else {
        if (preambleBits > MIN_PREAMBLE_BITS && !pendingData) {
          // ok, there was enough of preamble ones, let's start
          dataBits = 0;
        } else {
          // preamble too short
          restart();
        }
      }

    } else {
      int thisBit = dataBits++;

      int byteIndex = thisBit >> 3;
      int bitIndex = thisBit & 7;

      if (byteIndex < sizeof(buffer)) {

        if (!bitIndex) {
          buffer[byteIndex] = bit;
        } else {
          buffer[byteIndex] |= bit << bitIndex;
        }

        // complete byte
        if (bitIndex == 7) {

          // are we complete incl. crc?
          int len = buffer[2];
          if (byteIndex > 2 && 2 + 1 + len + 2 == byteIndex + 1) {
            int myCrc = 0x55;
            for (int c = 0; c < len; c++) {
              myCrc += buffer[c + 3];
            }
            int theirCrc = buffer[3 + len] | (buffer[3 + len + 1] << 8);
            if (theirCrc == myCrc) {
              pendingData = true;
              applicationEvents::schedule(rxEventId);
            } else {
              restart();
            }
          }
        }

      } else {
        restart();
      }
    }
  };
};

} // namespace rx
} // namespace ookey