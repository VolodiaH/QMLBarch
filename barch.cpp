#include "barch.hpp"

#include <stdexcept>
#include <fstream>
#include <cstring>
#include <climits>
#include <algorithm>
#include <iterator>
#include <QDebug>

namespace {

constexpr char kMagic0 = 'B';
constexpr char kMagic1 = 'A';
constexpr std::uint8_t kFileVersion = 0x01;
static_assert(CHAR_BIT == 8, "BARCH requires 8-bit bytes");
constexpr int kBitsPerByte = CHAR_BIT;
constexpr int kPixelsPerBlock = 4;
constexpr unsigned char kWhite = 0xFF;
constexpr unsigned char kBlack = 0x00;
constexpr unsigned char kPadPixelForCoding = kWhite;

struct TagBits
{
    static constexpr std::uint32_t WhiteVal = 0b0;  static constexpr int WhiteLen = 1;
    static constexpr std::uint32_t BlackVal = 0b10; static constexpr int BlackLen = 2;
    static constexpr std::uint32_t LiterVal = 0b11; static constexpr int LiterLen = 2;
};

template <typename T>
constexpr T ceilDiv(T a, T b) { return (a + b - 1) / b; }

inline void writeLE32(std::vector<std::uint8_t>& buf, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        buf.push_back(static_cast<std::uint8_t>(v >> (i * CHAR_BIT)));
}
inline std::uint32_t readLE32(const std::uint8_t* p)
{
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v |= (std::uint32_t)p[i] << (i * CHAR_BIT);
    return v;
}

inline bool isRowEmpty(const unsigned char* row, int width)
{
    for (int i = 0; i < width; ++i)
        if (row[i] != kWhite)
            return false;
    return true;
}

constexpr int kMSBIndex = kBitsPerByte - 1;

struct BitWriter
{
    std::vector<std::uint8_t> out;
    std::uint8_t cur = 0;
    int bitpos = 0;

    void putBit(int b)
    {
        cur |= std::uint8_t(b & 1) << (kMSBIndex - bitpos);
        if (++bitpos == kBitsPerByte)
        {
            out.push_back(cur);
            cur = 0;
            bitpos = 0;
        }
    }
    void putBits(std::uint32_t v, int n)
    {
        for (int i = n - 1; i >= 0; --i)
            putBit((v >> i) & 1);
    }
    void putByte(std::uint8_t b)
    {
        for (int i = kMSBIndex; i >= 0; --i)
            putBit((b >> i) & 1);
    }
    std::vector<std::uint8_t> finish()
    {
        if (bitpos)
            out.push_back(cur);
        return out;
    }
};

struct BitReader
{
    const std::uint8_t* p;
    std::size_t n;
    std::size_t idx = 0;
    int bitpos = 0;
    BitReader(const std::uint8_t* p_, std::size_t n_) : p(p_), n(n_) {}

    int getBit()
    {
        if (idx >= n)
        {
            qDebug() << "Unexpected end of bitstream";
            throw std::runtime_error("Unexpected end of bitstream");
        }

        int b = (p[idx] >> (kMSBIndex - bitpos)) & 1;
        if (++bitpos == kBitsPerByte)
        {
            bitpos = 0;
            ++idx;
        }
        return b;
    }
    std::uint32_t getBits(int k)
    {
        std::uint32_t v = 0;
        for (int i = 0; i < k; ++i)
            v = (v << 1) | getBit();
        return v;
    }
    std::uint8_t getByte() { return static_cast<std::uint8_t>(getBits(kBitsPerByte)); }
};

} // namespace

namespace barch
{

std::vector<std::uint8_t> encode(const RawImageData& img)
{
    if (!img.data || img.width <= 0 || img.height <= 0)
    {
        qDebug() << "encode: invalid input image";
        throw std::invalid_argument("encode: invalid input image");
    }

    const int W = img.width;
    const int H = img.height;
    const int rowIndexBytes = ceilDiv(H, kBitsPerByte);

    std::vector<std::uint8_t> rowIndex(rowIndexBytes, 0);
    std::vector<bool> nonEmpty(H, false);
    for (int y = 0; y < H; ++y)
    {
        const unsigned char* row = img.data + y * W;
        const bool empty = isRowEmpty(row, W);
        if (empty)
            rowIndex[y / kBitsPerByte] |= (1u << (y % kBitsPerByte));
        else nonEmpty[y] = true;
    }

    BitWriter bw;
    for (int y = 0; y < H; ++y)
    {
        if (!nonEmpty[y])
            continue;
        const unsigned char* row = img.data + y * W;
        const int groups = ceilDiv(W, kPixelsPerBlock);
        for (int g = 0; g < groups; ++g)
        {
            unsigned char px[kPixelsPerBlock];
            for (int k = 0; k < kPixelsPerBlock; ++k)
            {
                int x = g * kPixelsPerBlock + k;
                px[k] = (x < W) ? row[x] : kPadPixelForCoding;
            }
            bool allWhite = (px[0]==kWhite && px[1]==kWhite && px[2]==kWhite && px[3]==kWhite);
            bool allBlack = (px[0]==kBlack && px[1]==kBlack && px[2]==kBlack && px[3]==kBlack);
            if (allWhite)
                bw.putBits(TagBits::WhiteVal, TagBits::WhiteLen);
            else if (allBlack)
                bw.putBits(TagBits::BlackVal, TagBits::BlackLen);
            else
            {
                bw.putBits(TagBits::LiterVal, TagBits::LiterLen);
                bw.putByte(px[0]); bw.putByte(px[1]); bw.putByte(px[2]); bw.putByte(px[3]);
            }
        }
    }
    std::vector<std::uint8_t> bitstream = bw.finish();

    constexpr std::size_t kOffMagic0       = 0;
    constexpr std::size_t kOffMagic1       = 1;
    constexpr std::size_t kOffVersion      = 2;
    constexpr std::size_t kOffWidth        = 3;
    constexpr std::size_t kOffHeight       = 7;
    constexpr std::size_t kOffRowIndexSize = 11;
    constexpr std::size_t kOffDataSize     = 15;
    constexpr std::size_t kHeaderSize      = 19;

    std::vector<std::uint8_t> file;
    file.reserve(kHeaderSize + rowIndex.size() + bitstream.size());

    file.push_back(static_cast<std::uint8_t>(kMagic0));
    file.push_back(static_cast<std::uint8_t>(kMagic1));
    file.push_back(kFileVersion);
    writeLE32(file, static_cast<std::uint32_t>(W));
    writeLE32(file, static_cast<std::uint32_t>(H));
    writeLE32(file, static_cast<std::uint32_t>(rowIndex.size()));
    writeLE32(file, static_cast<std::uint32_t>(bitstream.size()));

    file.insert(file.end(), rowIndex.begin(), rowIndex.end());
    file.insert(file.end(), bitstream.begin(), bitstream.end());
    return file;
}

RawImageData decode(const std::uint8_t* bytes, std::size_t size)
{
    constexpr std::size_t kOffMagic0       = 0;
    constexpr std::size_t kOffMagic1       = 1;
    constexpr std::size_t kOffVersion      = 2;
    constexpr std::size_t kOffWidth        = 3;
    constexpr std::size_t kOffHeight       = 7;
    constexpr std::size_t kOffRowIndexSize = 11;
    constexpr std::size_t kOffDataSize     = 15;
    constexpr std::size_t kHeaderSize      = 19;

    if (!bytes || size < kHeaderSize)
    {
        qDebug() << "decode: too small";
        throw std::runtime_error("decode: too small");
    }
    if (bytes[kOffMagic0] != kMagic0 || bytes[kOffMagic1] != kMagic1)
    {
        qDebug() << "decode: bad magic";
        throw std::runtime_error("decode: bad magic");
    }
    if (bytes[kOffVersion] != kFileVersion)
    {
        qDebug() << "decode: unsupported version";
        throw std::runtime_error("decode: unsupported version");
    }

    const std::uint32_t W            = readLE32(bytes + kOffWidth);
    const std::uint32_t H            = readLE32(bytes + kOffHeight);
    const std::uint32_t rowIndexByte = readLE32(bytes + kOffRowIndexSize);
    const std::uint32_t dataBytes    = readLE32(bytes + kOffDataSize);

    const std::size_t need = kHeaderSize + rowIndexByte + dataBytes;
    if (size < need)
    {
        qDebug() << "decode: truncated file";
        throw std::runtime_error("decode: truncated file");
    }

    const std::uint8_t* rowIndex = bytes + kHeaderSize;
    const std::uint8_t* data     = rowIndex + rowIndexByte;

    const std::size_t total = static_cast<std::size_t>(W) * static_cast<std::size_t>(H);
    auto* outData = new unsigned char[total];

    BitReader br(data, dataBytes);
    for (std::uint32_t y = 0; y < H; ++y)
    {
        bool empty = (rowIndex[y / kBitsPerByte] >> (y % kBitsPerByte)) & 1;
        unsigned char* row = outData + y * W;
        if (empty)
            std::memset(row, kWhite, W); continue;

        std::uint32_t written = 0;
        while (written < W)
        {
            const int b0 = br.getBit();
            const int code = (b0 == 0) ? 0 : (2 | br.getBit()); // 0, 2, or 3
            const std::uint32_t n = std::min<std::uint32_t>(kPixelsPerBlock, W - written);

            switch (code)
            {
                case 0: std::memset(row + written, kWhite, n); written += n; break;
                case 2: std::memset(row + written, kBlack, n); written += n; break;
                case 3:
                {
                    unsigned char p[kPixelsPerBlock] = { br.getByte(), br.getByte(), br.getByte(), br.getByte() };
                    std::memcpy(row + written, p, n);
                    written += n;
                } break;
                default: {
                    qDebug() << "decode: invalid tag";
                    throw std::runtime_error("decode: invalid tag");}
            }
        }
    }

    RawImageData img;
    img.width  = static_cast<int>(W);
    img.height = static_cast<int>(H);
    img.data   = outData;
    return img;
}

void saveToFile(const std::string& path, const RawImageData& img)
{
    auto bytes = encode(img);
    std::ofstream f(path, std::ios::binary);
    if (!f)
    {
        qDebug() << "saveToFile: cannot open";
        throw std::runtime_error("saveToFile: cannot open");
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f)
    {
        qDebug() << "saveToFile: write failed";
        throw std::runtime_error("saveToFile: write failed");
    }
}

RawImageData loadFromFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        qDebug() << "loadFromFile: cannot open";
        throw std::runtime_error("loadFromFile: cannot open");
    }
    std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (buf.empty())
    {
        qDebug() << "loadFromFile: empty file";
        throw std::runtime_error("loadFromFile: empty file");
    }
    return decode(buf.data(), buf.size());
}

void freeImage(RawImageData& img)
{
    delete[] img.data;
    img.data = nullptr;
    img.width = img.height = 0;
}

} // barch
