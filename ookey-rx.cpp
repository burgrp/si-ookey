namespace ookey {
namespace rx {

class Decoder : public applicationEvents::EventHandler {

  const int MIN_PREAMBLE_BITS = 20; // there are 31 ones sent by transmitter,
                                    // let's be tollerant to noisy start

  union {
    unsigned char buffer[2 + 1 + 256 + 2];
    struct {
      unsigned short address;
      unsigned char length;
      unsigned char data[256];
    } their;
  };

  int dataBits = -1;
  int preambleBits = 0;
  bool pendingData = false;
  int rxEventId;

  virtual void dataReceived(unsigned short address, unsigned char *data,
                            int len) = 0;

  void onEvent() {
    dataReceived(their.address, their.data, their.length);
    restart();
  }

public:
  unsigned short address;
  bool promisc = false;

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

          if (byteIndex == 1 && !promisc && their.address != address) {
            restart();
          }

          // are we complete incl. crc?
          if (byteIndex > 2 && 2 + 1 + their.length + 2 == byteIndex + 1) {
            int myCrc = 0x55;
            for (int c = 0; c < their.length; c++) {
              myCrc += their.data[c];
            }
            int theirCrc =
                their.data[their.length] | (their.data[their.length + 1] << 8);
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