#ifndef __SKYDRAGON_DATA_H__
#define __SKYDRAGON_DATA_H__

#include <stdint.h>

namespace SkyDragon {

class Data
{
public:
    static const Data Null;

    Data();

    Data(const Data& other);

    Data(Data&& other);

    ~Data();

    Data& operator= (const Data& other);

    Data& operator= (Data&& other);

    unsigned char* getBytes() const;

    long getSize() const;

    void copy(const unsigned char* bytes, long size);

    void fastSet(unsigned char* bytes, long size);

    void clear();

    bool isNull() const;
private:
    void move(Data& other);

private:
    unsigned char* _bytes;
    long _size;
};

}

#endif // __SKYDRAGON_DATA_H__
