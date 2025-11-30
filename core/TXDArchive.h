#ifndef TXDARCHIVE_H
#define TXDARCHIVE_H

#include "TXDTextureHeader.h"
#include "TXDTypes.h"
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <map>

// Forward declaration for internal structure
struct TextureNativeData;

class TXDArchive {
public:
    TXDArchive();
    explicit TXDArchive(const std::string& filepath);
    explicit TXDArchive(std::istream& stream);
    ~TXDArchive();

    // Non-copyable
    TXDArchive(const TXDArchive&) = delete;
    TXDArchive& operator=(const TXDArchive&) = delete;

    // Texture access
    size_t getTextureCount() const { return textures.size(); }
    TXDTextureHeader* getTexture(size_t index);
    const TXDTextureHeader* getTexture(size_t index) const;
    TXDTextureHeader* findTexture(const std::string& name);
    const TXDTextureHeader* findTexture(const std::string& name) const;

    // Texture data
    std::unique_ptr<uint8_t[]> getTextureData(const TXDTextureHeader* header);
    void setTextureData(TXDTextureHeader* header, const uint8_t* data, size_t dataSize);
    void getOriginalDimensions(const TXDTextureHeader* header, uint16_t& width, uint16_t& height) const;

    // Texture management
    void addTexture(std::unique_ptr<TXDTextureHeader> header, const uint8_t* data = nullptr, size_t dataSize = 0);
    void removeTexture(size_t index);
    void removeTexture(const std::string& name);

    // File operations
    void load(const std::string& filepath);
    void load(std::istream& stream);
    void save(const std::string& filepath);
    void save(std::ostream& stream);

    // Apply changes (sync header changes to internal data)
    void applyTextureHeader(TXDTextureHeader* header);

    // Game version detection
    GTAGameVersion getGameVersion() const { return detectedGameVersion; }
    void setGameVersion(GTAGameVersion version) { detectedGameVersion = version; }

private:
    // Internal structure for texture data
    struct TextureData {
        std::unique_ptr<TXDTextureHeader> header;
        std::vector<uint8_t> data;
        uint16_t originalWidth;  // Original data dimensions (before user changes)
        uint16_t originalHeight;
    };
    
    std::vector<TextureData> textures;
    std::map<std::string, size_t> textureMap; // name -> index
    GTAGameVersion detectedGameVersion = GTAGameVersion::UNKNOWN;
    
    void readFromStream(std::istream& stream);
    void writeToStream(std::ostream& stream);
    
    // Helper functions
    void resizeTextureData(TextureData& texData);
    
    // Helper functions for reading TXD format
    bool readSection(std::istream& stream, uint32_t& sectionId, uint32_t& sectionSize, uint32_t& version);
    void readTextureNative(std::istream& stream, uint32_t sectionSize);
    void writeTextureNative(std::ostream& stream, const TextureData& texData);
    
    // Game version detection
    GTAGameVersion detectGameVersion(uint32_t packedVersion);
    uint32_t packGameVersion(GTAGameVersion gameVersion);
    
    // Constants
    static constexpr uint32_t RW_SECTION_TEXTUREDICTIONARY = 0x16;
    static constexpr uint32_t RW_SECTION_TEXTURENATIVE = 0x15;
    static constexpr uint32_t RW_SECTION_STRUCT = 0x01;
    static constexpr uint32_t RW_SECTION_EXTENSION = 0x03;
    static constexpr uint32_t RW_VERSION_GTASA = 0x34000;
};

#endif // TXDARCHIVE_H

