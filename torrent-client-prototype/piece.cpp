#include "byte_tools.h"
#include "piece.h"

namespace {
    constexpr size_t BLOCK_SIZE = 1 << 14;
}

Piece::Piece(size_t index, size_t length, std::string hash)
        : index_(index),
          length_(length),
          hash_(std::move(hash)) {
    int cnt = 0;
    while (length / BLOCK_SIZE > 0) {
        blocks_.push_back({static_cast<uint32_t>(index), static_cast<uint32_t>(cnt * BLOCK_SIZE), BLOCK_SIZE, Block::Status::Missing, std::string()});
        ++cnt;
        length -= BLOCK_SIZE;
    }
    if (length > 0)
        blocks_.push_back({static_cast<uint32_t>(index), static_cast<uint32_t>(cnt * BLOCK_SIZE), static_cast<uint32_t>(length), Block::Status::Missing, std::string()});
}

bool Piece::HashMatches() const {
    return hash_ == GetDataHash();
}

Block *Piece::FirstMissingBlock() {
    for (auto& block: blocks_) {
        if (block.status == Block::Status::Missing) {
            block.status = Block::Status::Pending;
            return &block;
        }
    }
    return nullptr;
}

size_t Piece::GetIndex() const {
    return index_;
}

void Piece::SaveBlock(size_t blockOffset, std::string data) {
    for (auto& block: blocks_) {
        if (block.offset == blockOffset) {
            block.status = Block::Status::Retrieved;
            block.data = std::move(data);
            return;
        }
    }
}

bool Piece::AllBlocksRetrieved() const {
    for (const auto& block: blocks_) {
        if (block.status != Block::Status::Retrieved)
            return false;
    }
    return true;
}

std::string Piece::GetData() const {
    std::string data;
    for (const auto& block: blocks_)
        data += block.data;
    return data;
}

std::string Piece::GetDataHash() const {
    return CalculateSHA1(GetData());
}

const std::string &Piece::GetHash() const {
    return hash_;
}

void Piece::Reset() {
    for (auto& block: blocks_)
        block.status = Block::Status::Missing;
    blocks_.clear();
}