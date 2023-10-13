#include "piece_storage.h"
#include <iostream>
#include <csignal>

PieceStorage::PieceStorage(const TorrentFile& tf, const std::filesystem::path& outputDirectory) :
        tf_(tf),
        outputDirectory_(outputDirectory) {
  //  while (outputFileStream_.tellp() == -1)
        initOutputFileStream();
   /* size_t allWithoutLastSizes = 0;
    for (int i = 0; i < tf.pieceHashes.size(); ++i) {
        allWithoutLastSizes += tf.pieceLength;
        remainPieces_.push(std::make_shared<Piece>(Piece(i,
                                                         allWithoutLastSizes +  tf.pieceLength < tf.length
                                                         ? tf.pieceLength
                                                         : tf.length - allWithoutLastSizes,
                                                         tf.pieceHashes[i])));
    }*/
    for (size_t i = 0; i < tf.length / tf.pieceLength; ++i) {
        size_t length = (i == tf.length / tf.pieceLength - 1) ? tf.length % tf.pieceLength : tf.pieceLength;
        remainPieces_.push(std::make_shared<Piece>(i, length, tf.pieceHashes[i]));
    }
}

void PieceStorage::initOutputFileStream() {
    std::cout << "Real position before: " << outputFileStream_.tellp() << "\n";
    std::cout << "badbit: " << std::boolalpha << (outputFileStream_.rdstate() == std::ios_base::badbit) << "\n";
    std::cout << "goodbit: " << std::boolalpha << (outputFileStream_.rdstate() == std::ios_base::goodbit) << "\n";
    std::cout << "failbit: " << std::boolalpha << (outputFileStream_.rdstate() == std::ios_base::failbit) << "\n";
    std::cout << "eofbit: " << std::boolalpha << (outputFileStream_.rdstate() == std::ios_base::eofbit) << "\n";
    std::cout << "tf length: " << tf_.length -  1 << "\n";
    outputFileStream_.close();
    outputFileStream_.open(outputDirectory_ / tf_.name, std::fstream ::out);
    outputFileStream_.seekp(tf_.length - 1);
    std::cout << "Real position after: " << outputFileStream_.tellp() << "\n";
    outputFileStream_.write("\0", 1);
    outputFileStream_.flush();
    std::cout << "Real position after after: " << outputFileStream_.tellp() << "\n";
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::lock_guard lock(mutex_);
    if (remainPieces_.empty())
        return nullptr;
    auto next = remainPieces_.front();
    remainPieces_.pop();
    return next;
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    std::lock_guard lock(mutex_);
    std::cout << "in piece processed \n";
    if (!piece->HashMatches()) {
        piece->Reset();
        return;
    }
    SavePieceToDisk(piece);
}

bool PieceStorage::QueueIsEmpty() const {
    std::lock_guard lock(mutex_);
    return remainPieces_.empty();
}

size_t PieceStorage::TotalPiecesCount() const {
    std::lock_guard lock(mutex_);
    return tf_.pieceHashes.size();
}

size_t PieceStorage::PiecesInProgressCount() const {
    std::lock_guard lock(mutex_);
    return tf_.pieceHashes.size() - remainPieces_.size();
}

void PieceStorage::CloseOutputFile() {
    std::lock_guard lock(mutex_);
    outputFileStream_.close();
}

const std::vector<size_t>& PieceStorage::GetPiecesSavedToDiscIndices() { // проблемный метод
    std::lock_guard lock(mutex_);
    return piecesSavedToDiscIndices_;
}

size_t PieceStorage::PiecesSavedToDiscCount() const {
    std::lock_guard lock(mutex_);
    return piecesSavedToDiscIndices_.size();
}

void PieceStorage::SavePieceToDisk(const PiecePtr &piece) {
    if (outputFileStream_.tellp() == -1)
        initOutputFileStream();
    piecesSavedToDiscIndices_.push_back(piece->GetIndex());
    std::cout << "Is open: " << outputFileStream_.is_open() << "\n";
  //  signal(SIGPIPE, SIG_IGN);
    std::cout << "Position: " << long(piece->GetIndex() * tf_.pieceLength) << "\n";
    std::cout << "Real position before: " << outputFileStream_.tellp() << "\n";
    std::cout << "badbit: " << std::boolalpha << (outputFileStream_.rdstate() == std::ios_base::badbit) << "\n";
    std::cout << "goodbit: " << std::boolalpha << (outputFileStream_.rdstate() == std::ios_base::goodbit) << "\n";
    std::cout << "failbit: " << std::boolalpha << (outputFileStream_.rdstate() == std::ios_base::failbit) << "\n";
    std::cout << "eofbit: " << std::boolalpha << (outputFileStream_.rdstate() == std::ios_base::eofbit) << "\n";
    outputFileStream_.seekp(long(piece->GetIndex() * tf_.pieceLength));
    std::cout << "Real position after: " << outputFileStream_.tellp() << "\n";
    std::cout << "piece size: " << piece->GetData().size() << "\n";
    outputFileStream_.write(piece->GetData().data(), long(piece->GetData().size()));
    outputFileStream_.flush();
    std::cout << "file size: " <<  std::filesystem::file_size(outputDirectory_ / tf_.name) << "\n";
    std::cout << "Was saved piece with index:  " << piece->GetIndex() << "\n";
}