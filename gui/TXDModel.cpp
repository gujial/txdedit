#include "TXDModel.h"
#include "libtxd/txd_dictionary.h"
#include "libtxd/txd_converter.h"
#include "libtxd/txd_texture.h"
#include <QPixmap>
#include <QImage>
#include <cstring>
#include <algorithm>

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
    entries.clear();
    gameVersion = LibTXD::GameVersion::UNKNOWN;
    version = 0;
    modified = false;
    filePath.clear();
    emit modelChanged();
}

TXDFileEntry* TXDModel::getTexture(size_t index) {
    if (index >= entries.size()) {
        return nullptr;
    }
    return &entries[index];
}

const TXDFileEntry* TXDModel::getTexture(size_t index) const {
    if (index >= entries.size()) {
        return nullptr;
    }
    return &entries[index];
}

TXDFileEntry* TXDModel::findTexture(const QString& name) {
    QString lowerName = name.toLower();
    for (auto& entry : entries) {
        if (entry.name.toLower() == lowerName) {
            return &entry;
        }
    }
    return nullptr;
}

const TXDFileEntry* TXDModel::findTexture(const QString& name) const {
    QString lowerName = name.toLower();
    for (const auto& entry : entries) {
        if (entry.name.toLower() == lowerName) {
            return &entry;
        }
    }
    return nullptr;
}

void TXDModel::addTexture(TXDFileEntry entry) {
    entries.push_back(std::move(entry));
    setModified(true);
    emit textureAdded(entries.size() - 1);
    emit modelChanged();
}

void TXDModel::removeTexture(size_t index) {
    if (index >= entries.size()) {
        return;
    }

    entries.erase(entries.begin() + index);
    setModified(true);
    emit textureRemoved(index);
    emit modelChanged();
}

void TXDModel::removeTexture(const QString& name) {
    QString lowerName = name.toLower();
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name.toLower() == lowerName) {
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

        TXDFileEntry entry;
        
        // Get metadata
        const auto& mipmap = libTexture->getMipmap(0);
        entry.name = QString::fromStdString(libTexture->getName());
        entry.maskName = QString::fromStdString(libTexture->getMaskName());
        entry.rasterFormat = libTexture->getRasterFormat();
        entry.compressionEnabled = (libTexture->getCompression() != LibTXD::Compression::NONE);
        entry.width = mipmap.width;
        entry.height = mipmap.height;
        entry.hasAlpha = libTexture->hasAlpha();
        entry.mipmapCount = libTexture->getMipmapCount();
        entry.filterFlags = libTexture->getFilterFlags();
        entry.isNew = false;  // Loaded from file
        
        // Decompress immediately and store as uncompressed RGBA
        LibTXD::Texture tempTexture;
        tempTexture.setName(libTexture->getName());
        tempTexture.setMaskName(libTexture->getMaskName());
        tempTexture.setRasterFormat(libTexture->getRasterFormat());
        tempTexture.setCompression(libTexture->getCompression());
        tempTexture.setHasAlpha(libTexture->hasAlpha());
        tempTexture.setFilterFlags(libTexture->getFilterFlags());
        
        LibTXD::MipmapLevel tempMipmap;
        tempMipmap.width = mipmap.width;
        tempMipmap.height = mipmap.height;
        tempMipmap.data = mipmap.data;
        tempMipmap.dataSize = mipmap.dataSize;
        tempTexture.addMipmap(std::move(tempMipmap));
        
        // Convert to RGBA8 (decompress if needed)
        auto rgbaData = LibTXD::TextureConverter::convertToRGBA8(tempTexture, 0);
        if (rgbaData) {
            size_t dataSize = entry.width * entry.height * 4;
            entry.diffuse.assign(rgbaData.get(), rgbaData.get() + dataSize);
        } else {
            // Conversion failed, skip this texture
            continue;
        }

        entries.push_back(std::move(entry));
    }

    return true;
}

std::unique_ptr<LibTXD::TextureDictionary> TXDModel::createDictionary() const {
    auto dict = std::make_unique<LibTXD::TextureDictionary>();
    dict->setVersion(version);

    for (const auto& entry : entries) {
        LibTXD::Texture texture;
        texture.setName(entry.name.toStdString());
        texture.setMaskName(entry.maskName.toStdString());
        texture.setRasterFormat(entry.rasterFormat);
        texture.setHasAlpha(entry.hasAlpha);
        texture.setFilterFlags(entry.filterFlags);

        // Determine compression based on compressionEnabled flag and alpha
        LibTXD::Compression comp = LibTXD::Compression::NONE;
        if (entry.compressionEnabled) {
            comp = entry.hasAlpha ? LibTXD::Compression::DXT3 : LibTXD::Compression::DXT1;
        }
        texture.setCompression(comp);

        // Compress on-the-fly when saving
        LibTXD::MipmapLevel mipmap;
        mipmap.width = entry.width;
        mipmap.height = entry.height;
        
        if (comp != LibTXD::Compression::NONE) {
            // Compress RGBA data
            auto compressedData = LibTXD::TextureConverter::compressToDXT(
                entry.diffuse.data(), entry.width, entry.height, comp, 1.0f);
            if (compressedData) {
                size_t compressedSize = LibTXD::TextureConverter::getCompressedDataSize(
                    entry.width, entry.height, comp);
                mipmap.data.assign(compressedData.get(), compressedData.get() + compressedSize);
                mipmap.dataSize = mipmap.data.size();
            } else {
                // Compression failed, use uncompressed
                mipmap.data = entry.diffuse;
                mipmap.dataSize = mipmap.data.size();
            }
        } else {
            // Uncompressed - use RGBA data directly
            mipmap.data = entry.diffuse;
            mipmap.dataSize = mipmap.data.size();
        }
        
        texture.addMipmap(std::move(mipmap));
        dict->addTexture(std::move(texture));
    }

    return dict;
}
