#include "TXDConverter.h"
#include <squish.h>
#include <libimagequant.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>



std::unique_ptr<uint8_t[]> TXDConverter::convertToRGBA8(
    const TXDTextureHeader* header,
    const uint8_t* data,
    int mipmapLevel) {
    
    if (!canConvert(header)) {
        return nullptr;
    }
    
    // Calculate mipmap dimensions
    float scale = std::pow(2.0f, static_cast<float>(mipmapLevel));
    int width = static_cast<int>(header->getWidth() / scale);
    int height = static_cast<int>(header->getHeight() / scale);
    
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    
    auto output = std::make_unique<uint8_t[]>(width * height * 4);
    
    // Calculate data offset for this mipmap
    size_t dataOffset = 0;
    
    // Skip palette if present
    if ((header->getRasterFormatExtension() & RasterFormatEXTPAL4) != 0) {
        dataOffset += 16 * 4;
    } else if ((header->getRasterFormatExtension() & RasterFormatEXTPAL8) != 0) {
        dataOffset += 256 * 4;
    }
    
    // Skip previous mipmaps
    for (int i = 0; i < mipmapLevel; i++) {
        dataOffset += header->computeMipmapDataSize(i);
    }
    
    const uint8_t* mipData = data + dataOffset;
    
    // Check for palette textures
    uint32_t rasterExt = header->getRasterFormatExtension();
    bool isPalette = (rasterExt & RasterFormatEXTPAL4) != 0 || (rasterExt & RasterFormatEXTPAL8) != 0;
    
    if (isPalette) {
        // Handle palette texture
        int paletteSize = (rasterExt & RasterFormatEXTPAL4) != 0 ? 16 : 256;
        const uint32_t* palette = reinterpret_cast<const uint32_t*>(mipData);
        const uint8_t* indexedData = mipData + (paletteSize * 4);
        convertPalette(indexedData, palette, paletteSize, width, height, output.get());
    } else {
        // Convert based on compression
        switch (header->getCompression()) {
            case TXDCompression::DXT1:
                convertDXT1(mipData, output.get(), width, height);
                break;
            case TXDCompression::DXT3:
                convertDXT3(mipData, output.get(), width, height);
                break;
            case TXDCompression::NONE:
                convertUncompressed(header, mipData, output.get(), mipmapLevel);
                break;
            default:
                // Unsupported compression
                std::memset(output.get(), 0, width * height * 4);
                break;
        }
    }
    
    return output;
}

bool TXDConverter::canConvert(const TXDTextureHeader* header) {
    // Check for palette textures
    uint32_t rasterExt = header->getRasterFormatExtension();
    bool isPalette = (rasterExt & RasterFormatEXTPAL4) != 0 || (rasterExt & RasterFormatEXTPAL8) != 0;
    if (isPalette) {
        return true;
    }
    
    // Support uncompressed, DXT1/DXT3
    bool supported = header->getCompression() == TXDCompression::NONE ||
                     header->getCompression() == TXDCompression::DXT1 ||
                     header->getCompression() == TXDCompression::DXT3;
    
    return supported;
}

void TXDConverter::convertUncompressed(
    const TXDTextureHeader* header,
    const uint8_t* data,
    uint8_t* output,
    int mipmapLevel) {
    
    float scale = std::pow(2.0f, static_cast<float>(mipmapLevel));
    int width = static_cast<int>(header->getWidth() / scale);
    int height = static_cast<int>(header->getHeight() / scale);
    
    uint32_t format = header->getRasterFormat();
    uint8_t bpp = header->getBytesPerPixel();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixelIndex = y * width + x;
            const uint8_t* pixelData = data + (pixelIndex * bpp);
            uint8_t* outPixel = output + (pixelIndex * 4);
            
            uint8_t r = 0, g = 0, b = 0, a = 255;
            
            switch (format) {
                case RasterFormatR8G8B8A8:
                    r = pixelData[0];
                    g = pixelData[1];
                    b = pixelData[2];
                    a = pixelData[3];
                    break;
                    
                case RasterFormatB8G8R8A8:
                    b = pixelData[0];
                    g = pixelData[1];
                    r = pixelData[2];
                    a = pixelData[3];
                    break;
                    
                case RasterFormatB8G8R8:
                    b = pixelData[0];
                    g = pixelData[1];
                    r = pixelData[2];
                    a = 255;
                    break;
                    
                case RasterFormatR5G6B5: {
                    uint16_t pixel = getPixel16(pixelData, 0);
                    r = ((pixel >> 11) & 0x1F) << 3;
                    g = ((pixel >> 5) & 0x3F) << 2;
                    b = (pixel & 0x1F) << 3;
                    a = 255;
                    break;
                }
                
                case RasterFormatA1R5G5B5: {
                    uint16_t pixel = getPixel16(pixelData, 0);
                    a = ((pixel >> 15) & 0x1) ? 255 : 0;
                    r = ((pixel >> 10) & 0x1F) << 3;
                    g = ((pixel >> 5) & 0x1F) << 3;
                    b = (pixel & 0x1F) << 3;
                    break;
                }
                
                case RasterFormatR4G4B4A4: {
                    uint16_t pixel = getPixel16(pixelData, 0);
                    r = ((pixel >> 12) & 0xF) << 4;
                    g = ((pixel >> 8) & 0xF) << 4;
                    b = ((pixel >> 4) & 0xF) << 4;
                    a = (pixel & 0xF) << 4;
                    break;
                }
                
                case RasterFormatLUM8:
                    r = g = b = pixelData[0];
                    a = 255;
                    break;
                    
                default:
                    r = g = b = 0;
                    a = 255;
                    break;
            }
            
            outPixel[0] = r;
            outPixel[1] = g;
            outPixel[2] = b;
            outPixel[3] = a;
        }
    }
}

void TXDConverter::convertDXT1(const uint8_t* data, uint8_t* output, int width, int height) {
    // Simple DXT1 decompression (basic implementation)
    // DXT1 uses 4x4 blocks, 8 bytes per block
    int blockWidth = (width + 3) / 4;
    int blockHeight = (height + 3) / 4;
    
    for (int by = 0; by < blockHeight; by++) {
        for (int bx = 0; bx < blockWidth; bx++) {
            const uint8_t* block = data + ((by * blockWidth + bx) * 8);
            
            uint16_t color0 = block[0] | (block[1] << 8);
            uint16_t color1 = block[2] | (block[3] << 8);
            uint32_t indices = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
            
            // Extract colors
            uint8_t r0 = ((color0 >> 11) & 0x1F) << 3;
            uint8_t g0 = ((color0 >> 5) & 0x3F) << 2;
            uint8_t b0 = (color0 & 0x1F) << 3;
            
            uint8_t r1 = ((color1 >> 11) & 0x1F) << 3;
            uint8_t g1 = ((color1 >> 5) & 0x3F) << 2;
            uint8_t b1 = (color1 & 0x1F) << 3;
            
            uint8_t r2, g2, b2, r3, g3, b3;
            if (color0 > color1) {
                r2 = (2 * r0 + r1) / 3;
                g2 = (2 * g0 + g1) / 3;
                b2 = (2 * b0 + b1) / 3;
                r3 = (r0 + 2 * r1) / 3;
                g3 = (g0 + 2 * g1) / 3;
                b3 = (b0 + 2 * b1) / 3;
            } else {
                r2 = (r0 + r1) / 2;
                g2 = (g0 + g1) / 2;
                b2 = (b0 + b1) / 2;
                r3 = g3 = b3 = 0;
            }
            
            uint8_t colors[4][3] = {
                {r0, g0, b0}, {r1, g1, b1}, {r2, g2, b2}, {r3, g3, b3}
            };
            
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = bx * 4 + x;
                    int py = by * 4 + y;
                    
                    if (px >= width || py >= height) continue;
                    
                    int index = (indices >> ((y * 4 + x) * 2)) & 0x3;
                    int outIndex = (py * width + px) * 4;
                    
                    output[outIndex + 0] = colors[index][0];
                    output[outIndex + 1] = colors[index][1];
                    output[outIndex + 2] = colors[index][2];
                    output[outIndex + 3] = (color0 <= color1 && index == 3) ? 0 : 255;
                }
            }
        }
    }
}

void TXDConverter::convertDXT3(const uint8_t* data, uint8_t* output, int width, int height) {
    // DXT3 uses 4x4 blocks, 16 bytes per block (8 bytes alpha + 8 bytes color)
    int blockWidth = (width + 3) / 4;
    int blockHeight = (height + 3) / 4;
    
    for (int by = 0; by < blockHeight; by++) {
        for (int bx = 0; bx < blockWidth; bx++) {
            const uint8_t* block = data + ((by * blockWidth + bx) * 16);
            
            // Extract alpha values (4 bits per pixel)
            uint64_t alphaBits = 0;
            for (int i = 0; i < 8; i++) {
                alphaBits |= (static_cast<uint64_t>(block[i]) << (i * 8));
            }
            
            // Extract color (same as DXT1)
            uint16_t color0 = block[8] | (block[9] << 8);
            uint16_t color1 = block[10] | (block[11] << 8);
            uint32_t indices = block[12] | (block[13] << 8) | (block[14] << 16) | (block[15] << 24);
            
            uint8_t r0 = ((color0 >> 11) & 0x1F) << 3;
            uint8_t g0 = ((color0 >> 5) & 0x3F) << 2;
            uint8_t b0 = (color0 & 0x1F) << 3;
            
            uint8_t r1 = ((color1 >> 11) & 0x1F) << 3;
            uint8_t g1 = ((color1 >> 5) & 0x3F) << 2;
            uint8_t b1 = (color1 & 0x1F) << 3;
            
            uint8_t r2 = (2 * r0 + r1) / 3;
            uint8_t g2 = (2 * g0 + g1) / 3;
            uint8_t b2 = (2 * b0 + b1) / 3;
            uint8_t r3 = (r0 + 2 * r1) / 3;
            uint8_t g3 = (g0 + 2 * g1) / 3;
            uint8_t b3 = (b0 + 2 * b1) / 3;
            
            uint8_t colors[4][3] = {
                {r0, g0, b0}, {r1, g1, b1}, {r2, g2, b2}, {r3, g3, b3}
            };
            
            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int px = bx * 4 + x;
                    int py = by * 4 + y;
                    
                    if (px >= width || py >= height) continue;
                    
                    int pixelIndex = y * 4 + x;
                    int alphaIndex = pixelIndex * 4;
                    uint8_t alpha = ((alphaBits >> alphaIndex) & 0xF) << 4;
                    
                    int index = (indices >> (pixelIndex * 2)) & 0x3;
                    int outIndex = (py * width + px) * 4;
                    
                    output[outIndex + 0] = colors[index][0];
                    output[outIndex + 1] = colors[index][1];
                    output[outIndex + 2] = colors[index][2];
                    output[outIndex + 3] = alpha;
                }
            }
        }
    }
}

uint16_t TXDConverter::getPixel16(const uint8_t* data, int index) {
    return data[index * 2] | (data[index * 2 + 1] << 8);
}

uint32_t TXDConverter::getPixel32(const uint8_t* data, int index) {
    return data[index * 4] | (data[index * 4 + 1] << 8) |
           (data[index * 4 + 2] << 16) | (data[index * 4 + 3] << 24);
}

size_t TXDConverter::getCompressedDataSize(int width, int height, TXDCompression compression) {
    switch (compression) {
        case TXDCompression::DXT1:
            return squish::GetStorageRequirements(width, height, squish::kDxt1);
        case TXDCompression::DXT3:
            return squish::GetStorageRequirements(width, height, squish::kDxt3);
        default:
            return 0;
    }
}

std::unique_ptr<uint8_t[]> TXDConverter::compressToDXT(
    const uint8_t* rgbaData,
    int width,
    int height,
    TXDCompression compression) {
    
    int flags = 0;
    switch (compression) {
        case TXDCompression::DXT1:
            flags = squish::kDxt1 | squish::kColourClusterFit;
            break;
        case TXDCompression::DXT3:
            flags = squish::kDxt3 | squish::kColourClusterFit;
            break;
        default:
            return nullptr;
    }
    
    size_t compressedSize = getCompressedDataSize(width, height, compression);
    if (compressedSize == 0) {
        return nullptr;
    }
    
    auto compressedData = std::make_unique<uint8_t[]>(compressedSize);
    
    // Compress the image using squish
    squish::CompressImage(rgbaData, width, height, compressedData.get(), flags);
    
    return compressedData;
}

bool TXDConverter::generatePalette(
    const uint8_t* rgbaData,
    int width,
    int height,
    int paletteSize,
    std::vector<uint32_t>& palette,
    std::vector<uint8_t>& indexedData) {
    
    // Create libimagequant attributes
    liq_attr* attr = liq_attr_create();
    if (!attr) {
        return false;
    }
    
    liq_set_max_colors(attr, paletteSize);
    liq_set_speed(attr, 5); // Balance between speed and quality
    
    // Create image from RGBA data
    liq_image* image = liq_image_create_rgba(attr, rgbaData, width, height, 0);
    if (!image) {
        liq_attr_destroy(attr);
        return false;
    }
    
    // Quantize the image
    liq_result* result = liq_quantize_image(attr, image);
    if (!result) {
        liq_image_destroy(image);
        liq_attr_destroy(attr);
        return false;
    }
    
    // Get palette
    const liq_palette* liq_pal = liq_get_palette(result);
    palette.clear();
    palette.reserve(paletteSize);
    
    for (int i = 0; i < liq_pal->count; i++) {
        uint32_t color = (liq_pal->entries[i].r) |
                        (liq_pal->entries[i].g << 8) |
                        (liq_pal->entries[i].b << 16) |
                        (liq_pal->entries[i].a << 24);
        palette.push_back(color);
    }
    
    // Pad palette to requested size if needed
    while (palette.size() < static_cast<size_t>(paletteSize)) {
        palette.push_back(0);
    }
    
    // Remap image to indexed
    indexedData.resize(width * height);
    liq_write_remapped_image(result, image, indexedData.data(), width * height);
    
    // Cleanup
    liq_result_destroy(result);
    liq_image_destroy(image);
    liq_attr_destroy(attr);
    
    return true;
}

void TXDConverter::convertPalette(
    const uint8_t* indexedData,
    const uint32_t* palette,
    int paletteSize,
    int width,
    int height,
    uint8_t* output) {
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = indexedData[y * width + x];
            if (index >= paletteSize) {
                index = 0; // Safety check
            }
            
            uint32_t color = palette[index];
            int outIndex = (y * width + x) * 4;
            
            output[outIndex + 0] = (color >> 0) & 0xFF;  // R
            output[outIndex + 1] = (color >> 8) & 0xFF;  // G
            output[outIndex + 2] = (color >> 16) & 0xFF; // B
            output[outIndex + 3] = (color >> 24) & 0xFF; // A
        }
    }
}

