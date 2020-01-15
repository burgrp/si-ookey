namespace ookey
{
namespace tx
{

class Encoder
{
    virtual void setTimerInterrupt(bool enabled) = 0;
    virtual void setRfPin(bool state) = 0;

    unsigned char buffer[
        4 + // preamble
        2 + // address
        1 + // size
        256 + // data
        2 // crc
    ];
    int counter = 0;
    int size = 0;

  public:
    void init(unsigned short address)
    {
        buffer[0] = 0xFF;
        buffer[1] = 0xFF;
        buffer[2] = 0xFF;
        buffer[3] = 0x7F;
        buffer[4] = address & 0xFF;
        buffer[5] = address >> 8;
    }

    // bool busy()
    // {
    //     return counter >> 4 < size;
    // }

    void send(unsigned char *data, int len)
    {
        int i = 6;
        buffer[i++] = len;
        int crc = 0x55;
        for (int c = 0; c < len; c++)
        {
            buffer[i++] = data[c];
            crc += data[c];
        }
        buffer[i++] = crc & 0xFF;
        buffer[i++] = (crc >> 8) & 0xFF;      

        size = i;
        counter = 0;

        setTimerInterrupt(true);
    }

    void handleTimerInterrupt()
    {
        int byteIdx = (counter >> 5);
        if (byteIdx < size)
        {            
            int bitIdx = (counter >> 2) & 0x07;

            int symbolIdx = counter & 3;

            setRfPin(symbolIdx == 0? 1: symbolIdx == 3? 0: ((buffer[byteIdx] >> bitIdx)  & 1));

            counter++;
        }
        else
        {
            setRfPin(0);
            setTimerInterrupt(false);
        }
    }
};

} // namespace tx
} // namespace ookey