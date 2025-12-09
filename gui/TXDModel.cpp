#include "TXDModel.h"
#include "libtxd/txd_dictionary.h"
#include "libtxd/txd_converter.h"
#include "libtxd/txd_texture.h"
#include <QPixmap>
#include <QImage>
#include <cstring>

TextureEntry::TextureEntry(QObject* parent)
    : QObject(parent)
    , rasterFormat(LibTXD::RasterFormat::DEFAULT)
    , compression(LibTXD::Compression::NONE)
    , width(0)
    , height(0)
    , hasAlphaChannel(false)
    , mipmapCount(0)
    , filterFlags(0)
{
}

void TextureEntry::setName(const QString& newName) {
    if (name != newName) {
        name = newName;
        emit nameChanged();
    }
}

void TextureEntry::setMaskName(const QString& newName) {
    if (maskName != newName) {
        maskName = newName;
        emit maskNameChanged();
    }
}

void TextureEntry::setHasAlpha(bool hasAlpha) {
    if (hasAlphaChannel != hasAlpha) {
        hasAlphaChannel = hasAlpha;
        
        // If compression requires update (DXT1 -> DXT3 or vice versa), update raw data
        if (compression == LibTXD::Compression::DXT1 && hasAlpha) {
            // Need to upgrade to DXT3 - get RGBA, set alpha, recompress
            updateRawDataForAlphaChange();
        } else if (compression == LibTXD::Compression::DXT3 && !hasAlpha) {
            // Can downgrade to DXT1 - get RGBA, set alpha to 255, compress as DXT1
            updateRawDataForAlphaChange();
        }
        
        updatePreviewPixmap(); // Rebuild preview
        emit hasAlphaChanged();
    }
}

void TextureEntry::updateRawDataForAlphaChange() {
    // Get current RGBA data
    LibTXD::Texture tempTexture;
    tempTexture.setName(name.toStdString());
    tempTexture.setRasterFormat(rasterFormat);
    tempTexture.setCompression(compression);
    tempTexture.setHasAlpha(hasAlphaChannel);
    
    if (!rawMipmapData.empty()) {
        LibTXD::MipmapLevel mipmap;
        mipmap.width = width;
        mipmap.height = height;
        mipmap.data = rawMipmapData;
        mipmap.dataSize = rawMipmapData.size();
        tempTexture.addMipmap(std::move(mipmap));
    }
    
    auto rgbaData = LibTXD::TextureConverter::convertToRGBA8(tempTexture, 0);
    if (!rgbaData) {
        return;
    }
    
    // Set alpha channel based on hasAlphaChannel flag
    size_t dataSize = width * height * 4;
    std::vector<uint8_t> newRGBA(dataSize);
    std::memcpy(newRGBA.data(), rgbaData.get(), dataSize);
    
    if (hasAlphaChannel) {
        // Set alpha to white (255) if enabling
        for (size_t i = 3; i < dataSize; i += 4) {
            newRGBA[i] = 255;
        }
    } else {
        // Set alpha to 255 (fully opaque) if disabling
        for (size_t i = 3; i < dataSize; i += 4) {
            newRGBA[i] = 255;
        }
    }
    
    // Determine new compression
    LibTXD::Compression newCompression = compression;
    if (hasAlphaChannel && compression == LibTXD::Compression::DXT1) {
        newCompression = LibTXD::Compression::DXT3;
    } else if (!hasAlphaChannel && compression == LibTXD::Compression::DXT3) {
        newCompression = LibTXD::Compression::DXT1;
    }
    
    // Recompress if needed
    if (newCompression != LibTXD::Compression::NONE) {
        auto compressedData = LibTXD::TextureConverter::compressToDXT(
            newRGBA.data(), width, height, newCompression, 1.0f);
        if (compressedData) {
            size_t compressedSize = LibTXD::TextureConverter::getCompressedDataSize(
                width, height, newCompression);
            rawMipmapData.assign(compressedData.get(), compressedData.get() + compressedSize);
            compression = newCompression;
        }
    } else {
        // Uncompressed - update directly
        rawMipmapData = newRGBA;
    }
}

void TextureEntry::setCompression(LibTXD::Compression comp) {
    if (compression != comp) {
        compression = comp;
        emit compressionChanged();
    }
}

void TextureEntry::setFilterFlags(uint32_t flags) {
    if (filterFlags != flags) {
        filterFlags = flags;
    }
}

void TextureEntry::setRawData(const std::vector<uint8_t>& rawMipmapData) {
    this->rawMipmapData = rawMipmapData;
    updatePreviewPixmap();
}

void TextureEntry::updateRawDataFromRGBA(const uint8_t* rgbaData, uint32_t w, uint32_t h, bool hasAlpha) {
    width = w;
    height = h;
    hasAlphaChannel = hasAlpha;
    
    // Compress if needed
    if (compression != LibTXD::Compression::NONE) {
        auto compressedData = LibTXD::TextureConverter::compressToDXT(
            rgbaData, w, h, compression, 1.0f);
        if (compressedData) {
            size_t compressedSize = LibTXD::TextureConverter::getCompressedDataSize(w, h, compression);
            rawMipmapData.assign(compressedData.get(), compressedData.get() + compressedSize);
        }
    } else {
        // Uncompressed
        size_t dataSize = w * h * 4;
        rawMipmapData.assign(rgbaData, rgbaData + dataSize);
    }
    
    updatePreviewPixmap();
}

void TextureEntry::setMetadata(const QString& name, const QString& maskName, LibTXD::RasterFormat format,
                                LibTXD::Compression comp, uint32_t w, uint32_t h, bool hasAlpha,
                                uint32_t mipmapCount, uint32_t filterFlags) {
    this->name = name;
    this->maskName = maskName;
    this->rasterFormat = format;
    this->compression = comp;
    this->width = w;
    this->height = h;
    this->hasAlphaChannel = hasAlpha;
    this->mipmapCount = mipmapCount;
    this->filterFlags = filterFlags;
}

void TextureEntry::updatePreviewPixmap() {
    // Create a temporary texture object for conversion
    LibTXD::Texture tempTexture;
    tempTexture.setName(name.toStdString());
    tempTexture.setMaskName(maskName.toStdString());
    tempTexture.setRasterFormat(rasterFormat);
    tempTexture.setCompression(compression);
    tempTexture.setHasAlpha(hasAlphaChannel);
    tempTexture.setFilterFlags(filterFlags);

    // Add mipmap with raw data
    if (!rawMipmapData.empty()) {
        LibTXD::MipmapLevel mipmap;
        mipmap.width = width;
        mipmap.height = height;
        mipmap.dataSize = rawMipmapData.size();
        mipmap.data = rawMipmapData;
        tempTexture.addMipmap(std::move(mipmap));
    }

    // Convert to RGBA8 for preview
    auto rgbaData = LibTXD::TextureConverter::convertToRGBA8(tempTexture, 0);
    if (rgbaData) {
        // Create QImage from RGBA data
        QImage image(rgbaData.get(), width, height, QImage::Format_RGBA8888);
        QImage imageCopy = image.copy(); // Make a copy since rgbaData will be freed
        
        // Create pixmap
        previewPixmap = QPixmap::fromImage(imageCopy);
        
        // Scale down if too large
        if (previewPixmap.width() > 512 || previewPixmap.height() > 512) {
            previewPixmap = previewPixmap.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        
        emit previewUpdated();
    } else {
        previewPixmap = QPixmap();
        emit previewUpdated();
    }
}

TXDModel::TXDModel(QObject* parent)
    : QObject(parent)
    , gameVersion(LibTXD::GameVersion::UNKNOWN)
    , version(0)
    , modified(false)
{
}

TXDModel::~TXDModel() {
    clear();
}

bool TXDModel::loadFromFile(const QString& filepath) {
    auto dict = std::make_unique<LibTXD::TextureDictionary>();
    if (!dict->load(filepath.toStdString())) {
        return false;
    }

    clear();
    
    if (loadFromDictionary(dict.get())) {
        filePath = filepath;
        gameVersion = dict->getGameVersion();
        version = dict->getVersion();
        modified = false;
        emit modelChanged();
        emit modifiedChanged(false);
        return true;
    }
    
    return false;
}

bool TXDModel::saveToFile(const QString& filepath) const {
    auto dict = createDictionary();
    if (!dict) {
        return false;
    }

    if (!dict->save(filepath.toStdString())) {
        return false;
    }

    return true;
}

void TXDModel::clear() {
    textures.clear();
    gameVersion = LibTXD::GameVersion::UNKNOWN;
    version = 0;
    modified = false;
    filePath.clear();
    emit modelChanged();
}

TextureEntry* TXDModel::getTexture(size_t index) {
    if (index >= textures.size()) {
        return nullptr;
    }
    return textures[index].get();
}

const TextureEntry* TXDModel::getTexture(size_t index) const {
    if (index >= textures.size()) {
        return nullptr;
    }
    return textures[index].get();
}

TextureEntry* TXDModel::findTexture(const QString& name) {
    for (auto& texture : textures) {
        if (texture->getName() == name) {
            return texture.get();
        }
    }
    return nullptr;
}

const TextureEntry* TXDModel::findTexture(const QString& name) const {
    for (const auto& texture : textures) {
        if (texture->getName() == name) {
            return texture.get();
        }
    }
    return nullptr;
}

void TXDModel::addTexture(TextureEntry* texture) {
    if (!texture) {
        return;
    }

    size_t index = textures.size();
    textures.emplace_back(texture);
    setModified(true);
    emit textureAdded(index);
    emit modelChanged();
}

void TXDModel::removeTexture(size_t index) {
    if (index >= textures.size()) {
        return;
    }

    textures.erase(textures.begin() + index);
    setModified(true);
    emit textureRemoved(index);
    emit modelChanged();
}

void TXDModel::removeTexture(const QString& name) {
    for (size_t i = 0; i < textures.size(); ++i) {
        if (textures[i]->getName() == name) {
            removeTexture(i);
            return;
        }
    }
}

void TXDModel::setModified(bool modified) {
    if (this->modified != modified) {
        this->modified = modified;
        emit modifiedChanged(modified);
    }
}

void TXDModel::setFilePath(const QString& path) {
    if (filePath != path) {
        filePath = path;
    }
}

bool TXDModel::loadFromDictionary(LibTXD::TextureDictionary* dict) {
    if (!dict) {
        return false;
    }

    for (size_t i = 0; i < dict->getTextureCount(); ++i) {
        const LibTXD::Texture* libTexture = dict->getTexture(i);
        if (!libTexture || libTexture->getMipmapCount() == 0) {
            continue;
        }

        auto entry = std::make_unique<TextureEntry>(this);
        
        // Get metadata
        const auto& mipmap = libTexture->getMipmap(0);
        entry->setMetadata(
            QString::fromStdString(libTexture->getName()),
            QString::fromStdString(libTexture->getMaskName()),
            libTexture->getRasterFormat(),
            libTexture->getCompression(),
            mipmap.width,
            mipmap.height,
            libTexture->hasAlpha(),
            libTexture->getMipmapCount(),
            libTexture->getFilterFlags()
        );

        // Get raw data (as-is from archive)
        entry->setRawData(mipmap.data); // Alpha is in same mipmap for libtxd

        textures.push_back(std::move(entry));
    }

    return true;
}

std::unique_ptr<LibTXD::TextureDictionary> TXDModel::createDictionary() const {
    auto dict = std::make_unique<LibTXD::TextureDictionary>();
    dict->setVersion(version);

    for (const auto& entry : textures) {
        LibTXD::Texture texture;
        texture.setName(entry->getName().toStdString());
        texture.setMaskName(entry->getMaskName().toStdString());
        texture.setRasterFormat(entry->getRasterFormat());
        texture.setCompression(entry->getCompression());
        texture.setHasAlpha(entry->hasAlpha());
        texture.setFilterFlags(entry->getFilterFlags());

        // Add mipmap with raw data
        LibTXD::MipmapLevel mipmap;
        mipmap.width = entry->getWidth();
        mipmap.height = entry->getHeight();
        mipmap.data = entry->getRawData();
        mipmap.dataSize = mipmap.data.size();
        texture.addMipmap(std::move(mipmap));

        dict->addTexture(std::move(texture));
    }

    return dict;
}
