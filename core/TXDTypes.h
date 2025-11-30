#ifndef TXDTYPES_H
#define TXDTYPES_H

#include <cstdint>
#include <string>

// Compression types
enum class TXDCompression : uint8_t {
    NONE = 0,
    DXT1 = 1,
    DXT3 = 3
};

// Raster formats
enum TXDRasterFormat : uint32_t {
    RasterFormatDefault = 0x000,
    RasterFormatA1R5G5B5 = 0x100,
    RasterFormatR5G6B5 = 0x200,
    RasterFormatR4G4B4A4 = 0x300,
    RasterFormatLUM8 = 0x400,
    RasterFormatB8G8R8A8 = 0x500,
    RasterFormatB8G8R8 = 0x600,
    RasterFormatR5G5B5 = 0xA00,
    RasterFormatR8G8B8A8 = 0xF00,
    
    RasterFormatEXTAutoMipmap = 0x1000,
    RasterFormatEXTPAL8 = 0x2000,
    RasterFormatEXTPAL4 = 0x4000,
    RasterFormatEXTMipmap = 0x8000,
    
    RasterFormatMask = 0xF00,
    RasterFormatEXTMask = 0xF000
};

// Filter flags
enum TXDFilterFlags : uint16_t {
    FilterNone = 0,
    FilterNearest = 1,
    FilterLinear = 2,
    FilterMipNearest = 3,
    FilterMipLinear = 4,
    FilterLinearMipNearest = 5,
    FilterLinearMipLinear = 6
};

// Wrapping modes
enum TXDWrappingMode : uint8_t {
    WrapNone = 0,
    WrapWrap = 1,
    WrapMirror = 2,
    WrapClamp = 3
};

// GTA game versions
enum class GTAGameVersion : uint8_t {
    UNKNOWN = 0,
    GTA3 = 1,
    GTAVC = 2,
    GTASA = 3
};

// Helper functions
inline uint32_t swapEndian32(uint32_t value) {
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

inline uint16_t swapEndian16(uint16_t value) {
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

inline uint32_t toLittleEndian32(uint32_t value) {
#ifdef __BIG_ENDIAN__
    return swapEndian32(value);
#else
    return value;
#endif
}

inline uint16_t toLittleEndian16(uint16_t value) {
#ifdef __BIG_ENDIAN__
    return swapEndian16(value);
#else
    return value;
#endif
}

inline uint32_t fromLittleEndian32(uint32_t value) {
#ifdef __BIG_ENDIAN__
    return swapEndian32(value);
#else
    return value;
#endif
}

inline uint16_t fromLittleEndian16(uint16_t value) {
#ifdef __BIG_ENDIAN__
    return swapEndian16(value);
#else
    return value;
#endif
}

#endif // TXDTYPES_H

