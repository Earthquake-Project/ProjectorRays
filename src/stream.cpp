#include <iostream>
#include <boost/endian/conversion.hpp>
#include <zlib.h>

#include "stream.h"
#include "util.h"

namespace ProjectorRays {

size_t ReadStream::pos() {
    return _pos;
}

size_t ReadStream::len() {
    return _len;
}

void ReadStream::seek(size_t pos) {
    _pos = pos;
}

void ReadStream::skip(size_t len) {
    _pos += len;
}

bool ReadStream::eof() {
    return  _pos >= _len || _offset + _pos >= _buf->size();
}

bool ReadStream::pastEOF() {
    return  _pos > _len || _offset + _pos > _buf->size();
}

uint8_t *ReadStream::getData() {
    return &_buf->data()[_offset];
}

std::shared_ptr<std::vector<uint8_t>> ReadStream::copyBytes(size_t len) {
    size_t p =  _offset + _pos;
    _pos += len;
    if (pastEOF())
        return nullptr;

    auto res = std::make_shared<std::vector<uint8_t>>(len, 0);
    res->assign(_buf->begin() + p, _buf->begin() + p + len);
    return res;
}

std::unique_ptr<ReadStream> ReadStream::readBytes(size_t len) {
    auto res = std::make_unique<ReadStream>(_buf, endianness, _offset + _pos, len);
    _pos += len;
    return res;
}

std::unique_ptr<ReadStream> ReadStream::readZlibBytes(unsigned long len, unsigned long *outLen) {
    size_t p = _offset + _pos;
    _pos += len;
    if (pastEOF())
        return nullptr;

    auto out = std::make_shared<std::vector<uint8_t>>(*outLen, 0);
    int ret = uncompress(out->data(), outLen, _buf->data() + p, len);
    if (ret != Z_OK)
        return nullptr;

    return std::make_unique<ReadStream>(out, endianness, 0, *outLen);
}

uint8_t ReadStream::readUint8() {
    size_t p = _offset + _pos;
    _pos += 1;
    if (pastEOF())
        return 0;

    return _buf->data()[p];
}

int8_t ReadStream::readInt8() {
    return (int8_t)readUint8();
}

uint16_t ReadStream::readUint16() {
    size_t p = _offset + _pos;
    _pos += 2;
    if (pastEOF())
        return 0;

    return endianness
        ? boost::endian::load_little_u16(reinterpret_cast<unsigned char *>(&_buf->data()[p]))
        : boost::endian::load_big_u16(reinterpret_cast<unsigned char *>(&_buf->data()[p]));
}

int16_t ReadStream::readInt16() {
    return (int16_t)readUint16();
}

uint32_t ReadStream::readUint32() {
    size_t p = _offset + _pos;
    _pos += 4;
    if (pastEOF())
        return 0;

    return endianness
        ? boost::endian::load_little_u32(reinterpret_cast<unsigned char *>(&_buf->data()[p]))
        : boost::endian::load_big_u32(reinterpret_cast<unsigned char *>(&_buf->data()[p]));
}

int32_t ReadStream::readInt32() {
    return (int32_t)readUint32();
}

double ReadStream::readDouble() {
    size_t p = _offset + _pos;
    _pos += 4;
    if (pastEOF())
        return 0;
    
    uint64_t f64bin = endianness
        ? boost::endian::load_little_u64(reinterpret_cast<unsigned char *>(&_buf->data()[p]))
        : boost::endian::load_big_u64(reinterpret_cast<unsigned char *>(&_buf->data()[p]));
    
    return *(double *)(&f64bin);
}

double ReadStream::readAppleFloat80() {
    // Adapted from @moralrecordings' code
    // from engines/director/lingo/lingo-bytecode.cpp in ScummVM

    // Floats are stored as an "80 bit IEEE Standard 754 floating
    // point number (Standard Apple Numeric Environment [SANE] data type
    // Extended).

    size_t p = _offset + _pos;
    _pos += 10;
    if (pastEOF())
        return 0.0;

    uint16_t exponent = boost::endian::load_big_u16(reinterpret_cast<unsigned char *>(&_buf->data()[p]));
    uint64_t f64sign = (uint64_t)(exponent & 0x8000) << 48;
    exponent &= 0x7fff;
    uint64_t fraction = boost::endian::load_big_u64(reinterpret_cast<unsigned char *>(&_buf->data()[p + 2]));
    fraction &= 0x7fffffffffffffffULL;
    uint64_t f64exp = 0;
    if (exponent == 0) {
        f64exp = 0;
    } else if (exponent == 0x7fff) {
        f64exp = 0x7ff;
    } else {
        int32_t normexp = (int32_t)exponent - 0x3fff;
        if ((-0x3fe > normexp) || (normexp >= 0x3ff)) {
            throw std::runtime_error("Constant float exponent too big for a double");
        }
        f64exp = (uint64_t)(normexp + 0x3ff);
    }
    f64exp <<= 52;
    uint64_t f64fract = fraction >> 11;
    uint64_t f64bin = f64sign | f64exp | f64fract;
    return *(double *)(&f64bin);
}

uint32_t ReadStream::readVarInt() {
	uint32_t val = 0;
	uint8_t b;
	do {
		b = readUint8();
		val = (val << 7) | (b & 0x7f); // The 7 least significant bits are appended to the result
	} while (b >> 7); // If the most significant bit is 1, there's another byte after
	return val;
}

std::string ReadStream::readString(size_t len) {
    size_t p = _offset + _pos;
    _pos += len;
    if (pastEOF())
        return "";

    char *str = new char[len + 1];
    memcpy(str, &_buf->data()[p], len);
    str[len] = '\0';
    std::string res(str);
    delete[] str;
    return res;
}

std::string ReadStream::readPascalString() {
    uint8_t len = readUint8();
    return readString(len);
}

}
