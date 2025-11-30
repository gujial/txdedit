#ifndef TXDCONVERTER_H
#define TXDCONVERTER_H

#include "TXDTextureHeader.h"
#include <memory>
#include <vector>

// Simple converter to convert TXD textures to RGBA format for display
class TXDConverter {
public:
    // Convert texture data to RGBA8 format (for display)
    // Returns nullptr on failure, or a buffer of width*height*4 bytes
    static std::unique_ptr<uint8_t[]> convertToRGBA8(
        const TXDTextureHeader* header,
        const uint8_t* data,
        int mipmapLevel = 0
    );
    
    // Check if conversion is supported
    static bool canConvert(const TXDTextureHeader* header);
    
    // Compress RGBA8 data to DXT format
    // Returns nullptr on failure, or a buffer with compressed data
    static std::unique_ptr<uint8_t[]> compressToDXT(
        const uint8_t* rgbaData,
        int width,
        int height,
        TXDCompression compression
    );
    
    // Get compressed data size for a given format and dimensions
    static size_t getCompressedDataSize(int width, int height, TXDCompression compression);
    
    // Generate palette from RGBA8 image data
    // Returns palette data (RGBA format) and indexed image data
    // paletteSize: 16 for PAL4, 256 for PAL8
    static bool generatePalette(
        const uint8_t* rgbaData,
        int width,
        int height,
        int paletteSize,
        std::vector<uint32_t>& palette,
        std::vector<uint8_t>& indexedData
    );
    
    // Convert palette texture to RGBA8
    static void convertPalette(
        const uint8_t* indexedData,
        const uint32_t* palette,
        int paletteSize,
        int width,
        int height,
        uint8_t* output
    );
    
private:
    static void convertUncompressed(
        const TXDTextureHeader* header,
        const uint8_t* data,
        uint8_t* output,
        int mipmapLevel
    );
    
    static void convertDXT1(
        const uint8_t* data,
        uint8_t* output,
        int width,
        int height
    );
    
    static void convertDXT3(
        const uint8_t* data,
        uint8_t* output,
        int width,
        int height
    );
    
    static uint16_t getPixel16(const uint8_t* data, int index);
    static uint32_t getPixel32(const uint8_t* data, int index);
};

#endif // TXDCONVERTER_H

