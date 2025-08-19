#include "bmp_io.h"
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>
#include <qDebug>

#pragma pack(push,1)
struct BMPHeader
{
    uint16_t bfType;      // 'BM' = 0x4D42
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
};
struct BMPInfoHeader
{
    uint32_t biSize;      // 40
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;    // 1
    uint16_t biBitCount;  // 8
    uint32_t biCompression; // 0 = BI_RGB
    uint32_t biSizeImage; // may be 0 for BI_RGB
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;      // 256
    uint32_t biClrImportant; // 256
};
#pragma pack(pop)

static inline uint32_t alignUp(uint32_t v, uint32_t a) { return ((v + a - 1) / a) * a; }

RawImageData loadGrayBMP(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        qDebug() << "loadGrayBMP: cannot open";
        throw std::runtime_error("loadGrayBMP: cannot open");
    }
    BMPHeader hdr{}; BMPInfoHeader info{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    f.read(reinterpret_cast<char*>(&info), sizeof(info));
    if (!f)
    {
        qDebug() << "loadGrayBMP: header read failed";
        throw std::runtime_error("loadGrayBMP: header read failed");
    }

    if (hdr.bfType != 0x4D42)
    {
        qDebug() << "loadGrayBMP: not BMP";
        throw std::runtime_error("loadGrayBMP: not BMP");
    }
    if (info.biBitCount != 8)
    {
        qDebug() << "loadGrayBMP: need 8-bit BMP";
        throw std::runtime_error("loadGrayBMP: need 8-bit BMP");
    }
    if (info.biCompression != 0)
    {
        qDebug() << "loadGrayBMP: compressed BMP not supported";
        throw std::runtime_error("loadGrayBMP: compressed BMP not supported");
    }

    const int W = info.biWidth;
    const int H = std::abs(info.biHeight);
    const bool bottomUp = (info.biHeight > 0);

    const int rowSize = alignUp(W, 4);

    f.seekg(hdr.bfOffBits, std::ios::beg);
    if (!f)
    {
        qDebug() << "loadGrayBMP: seek to pixels failed";
        throw std::runtime_error("loadGrayBMP: seek to pixels failed");
    }

    std::vector<unsigned char> buf(W * H);
    for (int y = 0; y < H; ++y)
    {
        int dstRow = bottomUp ? (H - 1 - y) : y;
        f.read(reinterpret_cast<char*>(&buf[dstRow * W]), W);
        f.ignore(rowSize - W);
        if (!f)
        {
            qDebug() << "loadGrayBMP: pixel read failed";
            throw std::runtime_error("loadGrayBMP: pixel read failed");
        }
    }

    auto* copy = new unsigned char[buf.size()];
    std::memcpy(copy, buf.data(), buf.size());
    return { W, H, copy };
}

void writeGrayBMP(const std::string& path, const RawImageData& img)
{
    if (img.width <= 0 || img.height <= 0 || !img.data)
        throw std::invalid_argument("writeGrayBMP: invalid image");

    const int W = img.width;
    const int H = img.height;
    const int rowSize = alignUp(W, 4);
    const uint32_t paletteSize   = 256 * 4; // BGRA
    const uint32_t pixelArraySize = rowSize * H;
    const uint32_t headerSize    = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + paletteSize;
    const uint32_t fileSize      = headerSize + pixelArraySize;

    BMPHeader hdr{};
    hdr.bfType   = 0x4D42;
    hdr.bfSize   = fileSize;
    hdr.bfOffBits = sizeof(BMPHeader) + sizeof(BMPInfoHeader) + paletteSize;

    BMPInfoHeader info{};
    info.biSize         = sizeof(BMPInfoHeader);
    info.biWidth        = W;
    info.biHeight       = H; // bottom-up
    info.biPlanes       = 1;
    info.biBitCount     = 8;
    info.biCompression  = 0; // BI_RGB
    info.biSizeImage    = pixelArraySize;
    info.biXPelsPerMeter = 2835; // ~72 DPI
    info.biYPelsPerMeter = 2835;
    info.biClrUsed       = 256;
    info.biClrImportant  = 256;

    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("writeGrayBMP: cannot open for write");

    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    f.write(reinterpret_cast<const char*>(&info), sizeof(info));

    for (int i = 0; i < 256; ++i)
    {
        unsigned char entry[4] = { (unsigned char)i, (unsigned char)i, (unsigned char)i, 0x00 };
        f.write(reinterpret_cast<const char*>(entry), 4);
    }

    std::vector<unsigned char> pad(rowSize - W, 0);
    for (int y = H - 1; y >= 0; --y)
    {
        const unsigned char* row = img.data + y * W;
        f.write(reinterpret_cast<const char*>(row), W);
        if (rowSize > W)
            f.write(reinterpret_cast<const char*>(pad.data()), rowSize - W);
    }
}
