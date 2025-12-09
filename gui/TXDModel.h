#ifndef TXD_MODEL_H
#define TXD_MODEL_H

#include <QObject>
#include <QString>
#include <QPixmap>
#include <vector>
#include <memory>
#include <string>
#include "libtxd/txd_types.h"

// Forward declarations
namespace LibTXD {
    class TextureDictionary;
    class Texture;
}

// Texture entry in the model - holds both raw data and preview data
class TextureEntry : public QObject {
    Q_OBJECT

public:
    TextureEntry(QObject* parent = nullptr);
    ~TextureEntry() = default;

    // Metadata
    QString getName() const { return name; }
    QString getMaskName() const { return maskName; }
    LibTXD::RasterFormat getRasterFormat() const { return rasterFormat; }
    LibTXD::Compression getCompression() const { return compression; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    bool hasAlpha() const { return hasAlphaChannel; }
    uint32_t getMipmapCount() const { return mipmapCount; }
    uint32_t getFilterFlags() const { return filterFlags; }

    // Raw data (as-is from archive - compressed or uncompressed mipmap data)
    const std::vector<uint8_t>& getRawData() const { return rawMipmapData; }

    // Preview data (merged RGBA for display)
    const QPixmap& getPreviewPixmap() const { return previewPixmap; }
    void updatePreviewPixmap(); // Rebuild preview from raw data

    // Setters (mutate model, update raw data and preview as needed, emit signals)
    void setName(const QString& newName);
    void setMaskName(const QString& newName);
    void setHasAlpha(bool hasAlpha); // Updates raw data if compression requires it, updates preview
    void setCompression(LibTXD::Compression comp);
    void setFilterFlags(uint32_t flags);

    // Update raw data from RGBA image data (used when importing/replacing)
    void updateRawDataFromRGBA(const uint8_t* rgbaData, uint32_t w, uint32_t h, bool hasAlpha);

    // Internal setters (for loading from archive) - public so TXDModel can use them
    void setRawData(const std::vector<uint8_t>& rawMipmapData);
    void setMetadata(const QString& name, const QString& maskName, LibTXD::RasterFormat format,
                    LibTXD::Compression comp, uint32_t w, uint32_t h, bool hasAlpha,
                    uint32_t mipmapCount, uint32_t filterFlags);

signals:
    void nameChanged();
    void maskNameChanged();
    void hasAlphaChanged();
    void compressionChanged();
    void previewUpdated();

private:
    // Metadata
    QString name;
    QString maskName;
    LibTXD::RasterFormat rasterFormat;
    LibTXD::Compression compression;
    uint32_t width;
    uint32_t height;
    bool hasAlphaChannel;
    uint32_t mipmapCount;
    uint32_t filterFlags;

    // Raw data (as stored in archive - compressed or uncompressed mipmap data)
    std::vector<uint8_t> rawMipmapData;

    // Preview data (merged RGBA for Qt display)
    QPixmap previewPixmap;

private:
    void updateRawDataForAlphaChange(); // Internal: update raw data when alpha state changes
};

// Main TXD model
class TXDModel : public QObject {
    Q_OBJECT

public:
    TXDModel(QObject* parent = nullptr);
    ~TXDModel();

    // File operations
    bool loadFromFile(const QString& filepath);
    bool saveToFile(const QString& filepath) const;
    void clear();

    // Metadata
    LibTXD::GameVersion getGameVersion() const { return gameVersion; }
    uint32_t getVersion() const { return version; }
    bool isModified() const { return modified; }
    QString getFilePath() const { return filePath; }

    // Texture access
    size_t getTextureCount() const { return textures.size(); }
    TextureEntry* getTexture(size_t index);
    const TextureEntry* getTexture(size_t index) const;
    TextureEntry* findTexture(const QString& name);
    const TextureEntry* findTexture(const QString& name) const;

    // Texture management
    void addTexture(TextureEntry* texture);
    void removeTexture(size_t index);
    void removeTexture(const QString& name);

    // Model state
    void setModified(bool modified);
    void setFilePath(const QString& path);

signals:
    void textureAdded(size_t index);
    void textureRemoved(size_t index);
    void textureUpdated(size_t index);
    void modelChanged();
    void modifiedChanged(bool modified);

private:
    // Load from LibTXD::TextureDictionary
    bool loadFromDictionary(LibTXD::TextureDictionary* dict);
    // Save to LibTXD::TextureDictionary
    std::unique_ptr<LibTXD::TextureDictionary> createDictionary() const;

    std::vector<std::unique_ptr<TextureEntry>> textures;
    LibTXD::GameVersion gameVersion;
    uint32_t version;
    bool modified;
    QString filePath;
};

#endif // TXD_MODEL_H
