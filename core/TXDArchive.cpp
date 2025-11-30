#include "TXDArchive.h"
#include "TXDTypes.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <iostream>

// Texture Native Structure (88 bytes)
struct TextureNativeStruct {
    int32_t platform;
    int16_t filterFlags;
    int8_t vWrap;
    int8_t uWrap;
    char diffuseName[32];
    char alphaName[32];
    int32_t rasterFormat;
    int32_t alphaOrCompression;
    int16_t width;
    int16_t height;
    int8_t bpp;
    int8_t mipmapCount;
    int8_t rasterType;
    int8_t compressionOrAlpha;
};

TXDArchive::TXDArchive() {
}

TXDArchive::TXDArchive(const std::string& filepath) {
    load(filepath);
}

TXDArchive::TXDArchive(std::istream& stream) {
    load(stream);
}

TXDArchive::~TXDArchive() = default;

void TXDArchive::load(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    load(file);
}

void TXDArchive::load(std::istream& stream) {
    textures.clear();
    textureMap.clear();
    readFromStream(stream);
}

void TXDArchive::save(const std::string& filepath) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    save(file);
}

void TXDArchive::save(std::ostream& stream) {
    writeToStream(stream);
}

bool TXDArchive::readSection(std::istream& stream, uint32_t& sectionId, uint32_t& sectionSize, uint32_t& version) {
    uint32_t header[3];
    stream.read(reinterpret_cast<char*>(header), 12);
    if (stream.gcount() != 12) {
        return false;
    }
    
    sectionId = fromLittleEndian32(header[0]);
    sectionSize = fromLittleEndian32(header[1]);
    version = fromLittleEndian32(header[2]);
    
    return true;
}

void TXDArchive::readTextureNative(std::istream& stream, uint32_t sectionSize) {
    size_t sectionStart = stream.tellg();
    size_t sectionEnd = sectionStart + sectionSize;
    
    // Read struct section
    uint32_t structId, structSize, structVersion;
    if (!readSection(stream, structId, structSize, structVersion)) {
        return;
    }
    
    if (structId != RW_SECTION_STRUCT) {
        throw std::runtime_error("Expected STRUCT section in TEXTURENATIVE");
    }
    
    // Read texture native structure
    TextureNativeStruct nativeStruct;
    if (stream.read(reinterpret_cast<char*>(&nativeStruct), sizeof(TextureNativeStruct)).gcount() != sizeof(TextureNativeStruct)) {
        throw std::runtime_error("Failed to read texture native structure");
    }
    
    // Convert endianness if needed
    nativeStruct.platform = fromLittleEndian32(nativeStruct.platform);
    nativeStruct.filterFlags = fromLittleEndian16(nativeStruct.filterFlags);
    nativeStruct.rasterFormat = fromLittleEndian32(nativeStruct.rasterFormat);
    nativeStruct.width = fromLittleEndian16(nativeStruct.width);
    nativeStruct.height = fromLittleEndian16(nativeStruct.height);
    nativeStruct.alphaOrCompression = fromLittleEndian32(nativeStruct.alphaOrCompression);
    
    // Determine compression
    TXDCompression compression = TXDCompression::NONE;
    bool alpha = false;
    
    if (nativeStruct.platform == 0x325350) { // "PS2"
        throw std::runtime_error("PS2 format is not supported");
    } else if (nativeStruct.platform == 9) {
        char* fourCC = reinterpret_cast<char*>(&nativeStruct.alphaOrCompression);
        if (fourCC[0] == 'D' && fourCC[1] == 'X' && fourCC[2] == 'T') {
            if (fourCC[3] == '1') {
                compression = TXDCompression::DXT1;
            } else if (fourCC[3] == '3') {
                compression = TXDCompression::DXT3;
            }
        }
        alpha = (nativeStruct.compressionOrAlpha == 9 || nativeStruct.compressionOrAlpha == 1);
    } else {
        if (nativeStruct.compressionOrAlpha == 1) {
            compression = TXDCompression::DXT1;
        } else if (nativeStruct.compressionOrAlpha == 3) {
            compression = TXDCompression::DXT3;
        }
        alpha = (fromLittleEndian32(nativeStruct.alphaOrCompression) == 1);
    }
    
    // Create texture header
    std::string diffuseName(nativeStruct.diffuseName, strnlen(nativeStruct.diffuseName, 32));
    std::string alphaName(nativeStruct.alphaName, strnlen(nativeStruct.alphaName, 32));
    
    auto header = std::make_unique<TXDTextureHeader>(
        diffuseName, nativeStruct.rasterFormat, compression,
        nativeStruct.width, nativeStruct.height
    );
    
    header->setAlphaChannelUsed(alpha);
    header->setAlphaName(alphaName);
    header->setFilterFlags(nativeStruct.filterFlags);
    header->setMipmapCount(nativeStruct.mipmapCount);
    header->setWrappingFlags(nativeStruct.uWrap, nativeStruct.vWrap);
    
    // Read texture data
    size_t dataSize = header->computeDataSize();
    std::vector<uint8_t> textureData(dataSize);
    
    // Skip palette if present
    size_t paletteSize = 0;
    if ((nativeStruct.rasterFormat & RasterFormatEXTPAL4) != 0) {
        paletteSize = 16 * 4;
    } else if ((nativeStruct.rasterFormat & RasterFormatEXTPAL8) != 0) {
        paletteSize = 256 * 4;
    }
    
    // Read mipmap data - it's stored after the struct, with size prefixes
    // The data is stored as: [size1][data1][size2][data2]...
    size_t offset = paletteSize;
    size_t dataStartPos = stream.tellg();
    
    for (int i = 0; i < nativeStruct.mipmapCount; i++) {
        int32_t mipSize;
        if (stream.read(reinterpret_cast<char*>(&mipSize), 4).gcount() != 4) {
            throw std::runtime_error("Failed to read mipmap size");
        }
        mipSize = fromLittleEndian32(mipSize);
        
        if (offset + mipSize > dataSize) {
            // Adjust data size if needed
            textureData.resize(offset + mipSize);
            dataSize = offset + mipSize;
        }
        
        if (stream.read(reinterpret_cast<char*>(textureData.data() + offset), mipSize).gcount() != static_cast<std::streamsize>(mipSize)) {
            throw std::runtime_error("Failed to read mipmap data");
        }
        offset += mipSize;
    }
    
    // Store texture
    TextureData texData;
    texData.header = std::move(header);
    texData.data = std::move(textureData);
    
    // Store original dimensions (from header at time of loading)
    texData.originalWidth = texData.header->getWidth();
    texData.originalHeight = texData.header->getHeight();
    
    std::string lowerName = diffuseName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    textureMap[lowerName] = textures.size();
    textures.push_back(std::move(texData));
    
    // Skip to end of section (there might be an extension section)
    stream.seekg(sectionEnd, std::ios::beg);
}

GTAGameVersion TXDArchive::detectGameVersion(uint32_t packedVersion) {
    // Only detect the three PC versions based on actual game file patterns
    // Everything else (PS2, Xbox, etc.) will return UNKNOWN
    
    uint16_t lower16 = static_cast<uint16_t>(packedVersion & 0xFFFF);
    uint16_t upper16 = static_cast<uint16_t>((packedVersion >> 16) & 0xFFFF);
    
    // Check for known hex patterns from actual PC game files
    // GTA3_PC: 0x0C02FFFF -> upper16 = 0x0C02, lower16 = 0xFFFF
    // VC_PC: 0x1003FFFF -> upper16 = 0x1003, lower16 = 0xFFFF
    // SA_PC: 0x1803FFFF -> upper16 = 0x1803, lower16 = 0xFFFF
    if (upper16 == 0x0C02 && lower16 == 0xFFFF) {
        return GTAGameVersion::GTA3;
    } else if (upper16 == 0x1003 && lower16 == 0xFFFF) {
        return GTAGameVersion::GTAVC;
    } else if (upper16 == 0x1803 && lower16 == 0xFFFF) {
        return GTAGameVersion::GTASA;
    }
    
    // All other formats (PS2, Xbox, etc.) return UNKNOWN
    return GTAGameVersion::UNKNOWN;
}

uint32_t TXDArchive::packGameVersion(GTAGameVersion gameVersion) {
    // Pack the game version into RenderWare version format
    // The version is stored as a 32-bit value with different packing for old/new style
    
    uint32_t packedVersion = 0;
    
    switch (gameVersion) {
        case GTAGameVersion::GTA3: {
            // GTA3_PC: rwLibMajor=3, rwLibMinor=1, rwRevMajor=1, rwRevMinor=0
            // Old style: [pad:6][packedLibraryMajorVer:2][packedReleaseMajorRev:4][packedReleaseMinorRev:4]
            // Bits: [15-10:pad=0][9-8:packedLibraryMajorVer=3][7-4:packedReleaseMajorRev=1][3-0:packedReleaseMinorRev=1]
            uint16_t libVer = (3 << 8) | (1 << 4) | 1; // packedLibraryMajorVer=3, packedReleaseMajorRev=1, packedReleaseMinorRev=1
            packedVersion = libVer; // Lower 16 bits for old style
            break;
        }
        case GTAGameVersion::GTAVC: {
            // VC_PC: rwLibMajor=3, rwLibMinor=4, rwRevMajor=3, rwRevMinor=0
            // Old style: [pad:6][packedLibraryMajorVer:2][packedReleaseMajorRev:4][packedReleaseMinorRev:4]
            uint16_t libVer = (3 << 8) | (4 << 4) | 3; // packedLibraryMajorVer=3, packedReleaseMajorRev=4, packedReleaseMinorRev=3
            packedVersion = libVer; // Lower 16 bits for old style
            break;
        }
        case GTAGameVersion::GTASA: {
            // SA: rwLibMajor=3, rwLibMinor=6, rwRevMajor=0, rwRevMinor=3
            // New style: [packedBinaryFormatRev:6][packedReleaseMinorRev:4][packedReleaseMajorRev:4][packedLibraryMajorVer:2]
            // packedLibraryMajorVer = rwLibMajor - 3 = 0
            // packedReleaseMajorRev = rwLibMinor = 6
            // packedReleaseMinorRev = rwRevMajor = 0
            // packedBinaryFormatRev = rwRevMinor = 3
            // Bits: [15-10:packedBinaryFormatRev=3][9-6:packedReleaseMinorRev=0][5-2:packedReleaseMajorRev=6][1-0:packedLibraryMajorVer=0]
            uint16_t libVer = (3 << 10) | (6 << 2); // packedBinaryFormatRev=3, packedReleaseMajorRev=6
            packedVersion = libVer; // Lower 16 bits for new style
            break;
        }
        default:
            // Default to SA version
            packedVersion = (3 << 10) | (6 << 2);
            break;
    }
    
    return packedVersion;
}

void TXDArchive::readFromStream(std::istream& stream) {
    uint32_t sectionId, sectionSize, version;
    if (!readSection(stream, sectionId, sectionSize, version)) {
        throw std::runtime_error("Failed to read TXD file header");
    }
    
    if (sectionId != RW_SECTION_TEXTUREDICTIONARY) {
        throw std::runtime_error("File is not a valid TXD archive");
    }
    
    // Detect game version from the main section version
    detectedGameVersion = detectGameVersion(version);
    
    // Track how much we've read within this section
    size_t sectionStart = stream.tellg();
    size_t sectionEnd = sectionStart + sectionSize;
    
    // Read child sections within the TEXTUREDICTIONARY section
    while (stream.tellg() < static_cast<std::streampos>(sectionEnd) && stream.good()) {
        uint32_t childId, childSize, childVersion;
        if (!readSection(stream, childId, childSize, childVersion)) {
            break;
        }
        
        size_t childStart = stream.tellg();
        size_t childEnd = childStart + childSize;
        
        if (childId == RW_SECTION_STRUCT) {
            // Read texture count
            int16_t textureCount;
            stream.read(reinterpret_cast<char*>(&textureCount), 2);
            textureCount = fromLittleEndian16(textureCount);
            int16_t unknown;
            stream.read(reinterpret_cast<char*>(&unknown), 2);
            // Skip to end of struct section
            stream.seekg(childEnd, std::ios::beg);
        } else if (childId == RW_SECTION_TEXTURENATIVE) {
            readTextureNative(stream, childSize);
            // Ensure we're at the end of the section
            stream.seekg(childEnd, std::ios::beg);
        } else if (childId == RW_SECTION_EXTENSION) {
            // Skip extension section
            stream.seekg(childSize, std::ios::cur);
        } else {
            // Unknown section, skip it
            stream.seekg(childSize, std::ios::cur);
        }
    }
}

void TXDArchive::writeTextureNative(std::ostream& stream, const TextureData& texData) {
    const TXDTextureHeader* header = texData.header.get();
    
    // Write TEXTURENATIVE section header
    uint32_t sectionId = toLittleEndian32(RW_SECTION_TEXTURENATIVE);
    uint32_t sectionSize = 0; // Will be calculated
    uint32_t version = toLittleEndian32(packGameVersion(detectedGameVersion));
    
    size_t sectionStart = stream.tellp();
    stream.write(reinterpret_cast<const char*>(&sectionId), 4);
    stream.write(reinterpret_cast<const char*>(&sectionSize), 4);
    stream.write(reinterpret_cast<const char*>(&version), 4);
    
    // Write STRUCT section
    uint32_t structId = toLittleEndian32(RW_SECTION_STRUCT);
    uint32_t structSize = toLittleEndian32(sizeof(TextureNativeStruct));
    stream.write(reinterpret_cast<const char*>(&structId), 4);
    stream.write(reinterpret_cast<const char*>(&structSize), 4);
    stream.write(reinterpret_cast<const char*>(&version), 4);
    
    // Build texture native structure
    TextureNativeStruct nativeStruct = {};
    nativeStruct.platform = toLittleEndian32(9);
    nativeStruct.filterFlags = toLittleEndian16(header->getFilterFlags());
    nativeStruct.vWrap = header->getVWrapFlags();
    nativeStruct.uWrap = header->getUWrapFlags();
    
    strncpy(nativeStruct.diffuseName, header->getDiffuseName().c_str(), 31);
    strncpy(nativeStruct.alphaName, header->getAlphaName().c_str(), 31);
    
    nativeStruct.rasterFormat = toLittleEndian32(header->getFullRasterFormat());
    nativeStruct.width = toLittleEndian16(header->getWidth());
    nativeStruct.height = toLittleEndian16(header->getHeight());
    nativeStruct.bpp = header->getBytesPerPixel() * 8;
    nativeStruct.mipmapCount = header->getMipmapCount();
    nativeStruct.rasterType = 0x4;
    
    // Set compression/alpha
    char* fourCC = reinterpret_cast<char*>(&nativeStruct.alphaOrCompression);
    switch (header->getCompression()) {
        case TXDCompression::DXT1:
            strncpy(fourCC, "DXT1", 4);
            break;
        case TXDCompression::DXT3:
            strncpy(fourCC, "DXT3", 4);
            break;
        case TXDCompression::NONE:
            *reinterpret_cast<int32_t*>(fourCC) = toLittleEndian32(header->isAlphaChannelUsed() ? 21 : 22);
            break;
        default:
            *reinterpret_cast<int32_t*>(fourCC) = toLittleEndian32(22);
            break;
    }
    
    nativeStruct.compressionOrAlpha = header->isAlphaChannelUsed() ? 
        (header->getCompression() == TXDCompression::NONE ? 1 : 9) : 
        (header->getCompression() == TXDCompression::NONE ? 0 : 8);
    
    stream.write(reinterpret_cast<const char*>(&nativeStruct), sizeof(TextureNativeStruct));
    
    // Write texture data
    size_t dataOffset = 0;
    size_t paletteSize = 0;
    if ((header->getRasterFormatExtension() & RasterFormatEXTPAL4) != 0) {
        paletteSize = 16 * 4;
    } else if ((header->getRasterFormatExtension() & RasterFormatEXTPAL8) != 0) {
        paletteSize = 256 * 4;
    }
    
    if (paletteSize > 0 && dataOffset + paletteSize <= texData.data.size()) {
        stream.write(reinterpret_cast<const char*>(texData.data.data() + dataOffset), paletteSize);
        dataOffset += paletteSize;
    }
    
    for (int i = 0; i < header->getMipmapCount(); i++) {
        int mipSize = header->computeMipmapDataSize(i);
        int32_t mipSizeLE = toLittleEndian32(static_cast<int32_t>(mipSize));
        stream.write(reinterpret_cast<const char*>(&mipSizeLE), 4);
        
        if (dataOffset + mipSize <= texData.data.size()) {
            stream.write(reinterpret_cast<const char*>(texData.data.data() + dataOffset), mipSize);
            dataOffset += mipSize;
        }
    }
    
    // Write extension section (empty)
    uint32_t extId = toLittleEndian32(RW_SECTION_EXTENSION);
    uint32_t extSize = toLittleEndian32(0);
    stream.write(reinterpret_cast<const char*>(&extId), 4);
    stream.write(reinterpret_cast<const char*>(&extSize), 4);
    stream.write(reinterpret_cast<const char*>(&version), 4);
    
    // Update section size
    size_t sectionEnd = stream.tellp();
    stream.seekp(sectionStart + 4);
    sectionSize = toLittleEndian32(static_cast<uint32_t>(sectionEnd - sectionStart - 12));
    stream.write(reinterpret_cast<const char*>(&sectionSize), 4);
    stream.seekp(sectionEnd);
}

void TXDArchive::writeToStream(std::ostream& stream) {
    // Write TEXTUREDICTIONARY section header
    uint32_t sectionId = toLittleEndian32(RW_SECTION_TEXTUREDICTIONARY);
    uint32_t sectionSize = 0; // Will be calculated
    uint32_t version = toLittleEndian32(packGameVersion(detectedGameVersion));
    
    size_t sectionStart = stream.tellp();
    stream.write(reinterpret_cast<const char*>(&sectionId), 4);
    stream.write(reinterpret_cast<const char*>(&sectionSize), 4);
    stream.write(reinterpret_cast<const char*>(&version), 4);
    
    // Write STRUCT section (texture count)
    uint32_t structId = toLittleEndian32(RW_SECTION_STRUCT);
    uint32_t structSize = toLittleEndian32(4);
    stream.write(reinterpret_cast<const char*>(&structId), 4);
    stream.write(reinterpret_cast<const char*>(&structSize), 4);
    stream.write(reinterpret_cast<const char*>(&version), 4);
    
    int16_t textureCount = toLittleEndian16(static_cast<int16_t>(textures.size()));
    int16_t unknown = 0;
    stream.write(reinterpret_cast<const char*>(&textureCount), 2);
    stream.write(reinterpret_cast<const char*>(&unknown), 2);
    
    // Write all textures
    for (const auto& texData : textures) {
        writeTextureNative(stream, texData);
    }
    
    // Write extension section (empty)
    uint32_t extId = toLittleEndian32(RW_SECTION_EXTENSION);
    uint32_t extSize = toLittleEndian32(0);
    stream.write(reinterpret_cast<const char*>(&extId), 4);
    stream.write(reinterpret_cast<const char*>(&extSize), 4);
    stream.write(reinterpret_cast<const char*>(&version), 4);
    
    // Update section size
    size_t sectionEnd = stream.tellp();
    stream.seekp(sectionStart + 4);
    sectionSize = toLittleEndian32(static_cast<uint32_t>(sectionEnd - sectionStart - 12));
    stream.write(reinterpret_cast<const char*>(&sectionSize), 4);
    stream.seekp(sectionEnd);
}

TXDTextureHeader* TXDArchive::getTexture(size_t index) {
    if (index >= textures.size()) {
        return nullptr;
    }
    return textures[index].header.get();
}

const TXDTextureHeader* TXDArchive::getTexture(size_t index) const {
    if (index >= textures.size()) {
        return nullptr;
    }
    return textures[index].header.get();
}

TXDTextureHeader* TXDArchive::findTexture(const std::string& name) {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    auto it = textureMap.find(lowerName);
    if (it != textureMap.end()) {
        return textures[it->second].header.get();
    }
    return nullptr;
}

const TXDTextureHeader* TXDArchive::findTexture(const std::string& name) const {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    auto it = textureMap.find(lowerName);
    if (it != textureMap.end()) {
        return textures[it->second].header.get();
    }
    return nullptr;
}

std::unique_ptr<uint8_t[]> TXDArchive::getTextureData(const TXDTextureHeader* header) {
    for (const auto& texData : textures) {
        if (texData.header.get() == header) {
            auto data = std::make_unique<uint8_t[]>(texData.data.size());
            std::memcpy(data.get(), texData.data.data(), texData.data.size());
            return data;
        }
    }
    return nullptr;
}

void TXDArchive::setTextureData(TXDTextureHeader* header, const uint8_t* data, size_t dataSize) {
    for (auto& texData : textures) {
        if (texData.header.get() == header) {
            texData.data.assign(data, data + dataSize);
            // Update original dimensions when data is set
            texData.originalWidth = header->getWidth();
            texData.originalHeight = header->getHeight();
            return;
        }
    }
}

void TXDArchive::getOriginalDimensions(const TXDTextureHeader* header, uint16_t& width, uint16_t& height) const {
    for (const auto& texData : textures) {
        if (texData.header.get() == header) {
            width = texData.originalWidth;
            height = texData.originalHeight;
            return;
        }
    }
    // Fallback to header dimensions if not found
    width = header->getWidth();
    height = header->getHeight();
}

void TXDArchive::addTexture(std::unique_ptr<TXDTextureHeader> header, const uint8_t* data, size_t dataSize) {
    TextureData texData;
    texData.header = std::move(header);
    
    if (data && dataSize > 0) {
        texData.data.assign(data, data + dataSize);
    } else {
        size_t expectedSize = texData.header->computeDataSize();
        texData.data.resize(expectedSize, 0);
    }
    
    // Store original dimensions (from header at time of addition)
    texData.originalWidth = texData.header->getWidth();
    texData.originalHeight = texData.header->getHeight();
    
    std::string lowerName = texData.header->getDiffuseName();
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    textureMap[lowerName] = textures.size();
    textures.push_back(std::move(texData));
}

void TXDArchive::removeTexture(size_t index) {
    if (index >= textures.size()) {
        return;
    }
    
    std::string lowerName = textures[index].header->getDiffuseName();
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    textureMap.erase(lowerName);
    textures.erase(textures.begin() + index);
    
    // Rebuild map
    textureMap.clear();
    for (size_t i = 0; i < textures.size(); i++) {
        std::string name = textures[i].header->getDiffuseName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        textureMap[name] = i;
    }
}

void TXDArchive::removeTexture(const std::string& name) {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    auto it = textureMap.find(lowerName);
    if (it != textureMap.end()) {
        removeTexture(it->second);
    }
}

void TXDArchive::applyTextureHeader(TXDTextureHeader* header) {
    // Header changes are already reflected in the TXDTextureHeader object
    // This method exists for compatibility with the reference API
    // In our simplified version, we don't need to do anything special
}

