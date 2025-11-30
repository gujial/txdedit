#include "TXDTextureHeader.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <stdexcept>

TXDTextureHeader::TXDTextureHeader(const std::string& diffuseName, uint32_t rasterFormat,
                                   TXDCompression compression, uint16_t w, uint16_t h)
    : diffuseName(diffuseName), rasterFormat(rasterFormat), compression(compression),
      width(w), height(h), mipmapCount(1), alphaChannelUsed(false),
      uWrap(WrapNone), vWrap(WrapNone), filterFlags(FilterNone) {
    setRasterFormat(rasterFormat, compression);
    setDiffuseName(diffuseName);
}

TXDTextureHeader::TXDTextureHeader(const TXDTextureHeader& other)
    : diffuseName(other.diffuseName), alphaName(other.alphaName),
      rasterFormat(other.rasterFormat), compression(other.compression),
      width(other.width), height(other.height), mipmapCount(other.mipmapCount),
      alphaChannelUsed(other.alphaChannelUsed), uWrap(other.uWrap),
      vWrap(other.vWrap), filterFlags(other.filterFlags) {
}

uint8_t TXDTextureHeader::getBytesPerPixel() const {
    if ((rasterFormat & RasterFormatEXTPAL4) != 0 || (rasterFormat & RasterFormatEXTPAL8) != 0) {
        return 1;
    }
    
    switch (getRasterFormat()) {
        case RasterFormatB8G8R8A8:
        case RasterFormatR8G8B8A8:
        case RasterFormatB8G8R8:
            return 4;
        case RasterFormatA1R5G5B5:
        case RasterFormatR4G4B4A4:
        case RasterFormatR5G5B5:
        case RasterFormatR5G6B5:
            return 2;
        case RasterFormatLUM8:
            return 1;
        default:
            return 0;
    }
}

int TXDTextureHeader::computeDataSize() const {
    int size = 0;
    for (uint8_t i = 0; i < mipmapCount; i++) {
        size += computeMipmapDataSize(i);
    }
    
    if ((rasterFormat & RasterFormatEXTPAL4) != 0) {
        size += 16 * 4;
    }
    if ((rasterFormat & RasterFormatEXTPAL8) != 0) {
        size += 256 * 4;
    }
    
    return size;
}

int TXDTextureHeader::computeMipmapDataSize(int mipmap) const {
    uint8_t bpp = getBytesPerPixel();
    float scale = std::ceil(std::pow(2.0f, static_cast<float>(mipmap)));
    int mipW = static_cast<int>(width / scale);
    int mipH = static_cast<int>(height / scale);
    
    if (compression == TXDCompression::DXT1 || compression == TXDCompression::DXT3) {
        if (mipW < 4) mipW = 4;
        if (mipH < 4) mipH = 4;
    }
    
    int mipSize = mipW * mipH;
    
    switch (compression) {
        case TXDCompression::NONE:
            mipSize *= bpp;
            break;
        case TXDCompression::DXT1:
            mipSize /= 2;
            break;
        case TXDCompression::DXT3:
            break;
        default:
            // Unsupported compression format
            mipSize = 0;
            break;
    }
    
    return mipSize;
}

int8_t TXDTextureHeader::calculateMaximumMipmapLevel() {
    int minW, minH;
    
    switch (compression) {
        case TXDCompression::DXT1:
        case TXDCompression::DXT3:
            minW = 4;
            minH = 4;
            break;
        default:
            minW = 1;
            minH = 1;
    }
    
    return static_cast<int8_t>(std::min(
        -(std::log10(static_cast<float>(minW) / width) / std::log10(2.0f)),
        -(std::log10(static_cast<float>(minH) / height) / std::log10(2.0f))
    ));
}

void TXDTextureHeader::fixMipmapCount() {
    mipmapCount = std::min(static_cast<uint8_t>(calculateMaximumMipmapLevel() + 1), mipmapCount);
}

void TXDTextureHeader::setRasterFormat(uint32_t rasterFormat, TXDCompression compression) {
    if (rasterFormat == RasterFormatDefault) {
        switch (compression) {
            case TXDCompression::DXT1:
                rasterFormat = RasterFormatR5G6B5;
                break;
            case TXDCompression::DXT3:
                rasterFormat = RasterFormatR4G4B4A4;
                break;
            case TXDCompression::NONE:
                throw std::runtime_error("Raster format DEFAULT is invalid for uncompressed textures!");
            default:
                // Unsupported compression format
                break;
        }
    }
    
    this->rasterFormat = rasterFormat;
    this->compression = compression;
    
    if (calculateMaximumMipmapLevel() < 0) {
        throw std::runtime_error("Invalid texture dimensions for this format!");
    }
}

void TXDTextureHeader::setDiffuseName(const std::string& name) {
    if (name.length() > 31) {
        throw std::runtime_error("Texture diffuse name too long. Maximum length is 31 characters.");
    }
    diffuseName = name;
}

void TXDTextureHeader::setAlphaName(const std::string& name) {
    if (name.length() > 31) {
        throw std::runtime_error("Texture alpha name too long. Maximum length is 31 characters.");
    }
    alphaName = name;
}

void TXDTextureHeader::getFormat(char* dest) const {
    const char* compStr = "";
    switch (compression) {
        case TXDCompression::NONE:
            compStr = "un";
            break;
        case TXDCompression::DXT1:
            compStr = "DXT1-";
            break;
        case TXDCompression::DXT3:
            compStr = "DXT3-";
            break;
        default:
            compStr = "unsupported-";
            break;
    }
    
    const char* formatStr = "";
    switch (getRasterFormat()) {
        case RasterFormatA1R5G5B5:
            formatStr = "A1R5G5B5";
            break;
        case RasterFormatR5G6B5:
            formatStr = "R5G6B5";
            break;
        case RasterFormatR4G4B4A4:
            formatStr = "R4G4B4A4";
            break;
        case RasterFormatLUM8:
            formatStr = "LUM8";
            break;
        case RasterFormatB8G8R8A8:
            formatStr = "B8G8R8A8";
            break;
        case RasterFormatB8G8R8:
            formatStr = "B8G8R8";
            break;
        case RasterFormatR5G5B5:
            formatStr = "R5G5B5";
            break;
        case RasterFormatR8G8B8A8:
            formatStr = "R8G8B8A8";
            break;
        default:
            formatStr = "DEFAULT";
            break;
    }
    
    std::snprintf(dest, 256, "%dx%d@%d %scompressed containing %d mipmaps with%s alpha in format %s",
                  width, height, getBytesPerPixel(), compStr, mipmapCount,
                  alphaChannelUsed ? "" : "out", formatStr);
}

