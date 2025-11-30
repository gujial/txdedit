#ifndef TXDTEXTUREHEADER_H
#define TXDTEXTUREHEADER_H

#include "TXDTypes.h"
#include <string>
#include <cstdint>
#include <memory>

class TXDTextureHeader {
public:
    TXDTextureHeader(const std::string& diffuseName, uint32_t rasterFormat, 
                     TXDCompression compression, uint16_t width, uint16_t height);
    TXDTextureHeader(const TXDTextureHeader& other);
    ~TXDTextureHeader() = default;

    // Getters
    const std::string& getDiffuseName() const { return diffuseName; }
    const std::string& getAlphaName() const { return alphaName; }
    uint32_t getRasterFormat() const { return rasterFormat & RasterFormatMask; }
    uint32_t getFullRasterFormat() const { return rasterFormat; }
    uint32_t getRasterFormatExtension() const { return rasterFormat & RasterFormatEXTMask; }
    TXDCompression getCompression() const { return compression; }
    uint16_t getWidth() const { return width; }
    uint16_t getHeight() const { return height; }
    uint8_t getBytesPerPixel() const;
    uint8_t getMipmapCount() const { return mipmapCount; }
    bool isAlphaChannelUsed() const { return alphaChannelUsed; }
    uint8_t getUWrapFlags() const { return uWrap; }
    uint8_t getVWrapFlags() const { return vWrap; }
    uint16_t getFilterFlags() const { return filterFlags; }

    // Setters
    void setRasterFormat(uint32_t rasterFormat, TXDCompression compression = TXDCompression::NONE);
    void setDiffuseName(const std::string& name);
    void setAlphaName(const std::string& name);
    void setRasterSize(uint16_t w, uint16_t h) { width = w; height = h; }
    void setMipmapCount(uint8_t mmc) { mipmapCount = mmc; }
    void setAlphaChannelUsed(bool alpha) { alphaChannelUsed = alpha; }
    void setWrappingFlags(uint8_t uWrap, uint8_t vWrap) { this->uWrap = uWrap; this->vWrap = vWrap; }
    void setFilterFlags(uint16_t flags) { filterFlags = flags; }

    // Utility
    int computeDataSize() const;
    int computeMipmapDataSize(int mipmap) const;
    int8_t calculateMaximumMipmapLevel();
    void fixMipmapCount();
    void getFormat(char* dest) const;

private:
    std::string diffuseName;
    std::string alphaName;
    uint32_t rasterFormat;
    TXDCompression compression;
    uint16_t width;
    uint16_t height;
    uint8_t mipmapCount;
    bool alphaChannelUsed;
    uint8_t uWrap;
    uint8_t vWrap;
    uint16_t filterFlags;
};

#endif // TXDTEXTUREHEADER_H

