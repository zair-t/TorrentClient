#include "peer_connect.h"
#include "message.h"
#include "byte_tools.h"
#include <iostream>
#include <utility>
#include <cstring>

using namespace std::chrono_literals;

PeerPiecesAvailability::PeerPiecesAvailability() = default;

PeerPiecesAvailability::PeerPiecesAvailability(std::string bitfield)
        : bitfield_(std::move(bitfield)) {}

bool PeerPiecesAvailability::IsPieceAvailable(size_t pieceIndex) const {
    return ((bitfield_[pieceIndex / 8] >> (7 - pieceIndex % 8) & 1)) != 0;
}

void PeerPiecesAvailability::SetPieceAvailability(size_t pieceIndex) {
    bitfield_[pieceIndex / 8] = char(bitfield_[pieceIndex / 8] | (1 << (7 - pieceIndex % 8)));
}

size_t PeerPiecesAvailability::Size() const {
    return bitfield_.size() * 8;
}

PeerConnect::PeerConnect(const Peer &peer, const TorrentFile &tf, std::string selfPeerId, PieceStorage &pieceStorage)
        : tf_(tf),
          socket_(peer.ip, peer.port, 1000ms, 1000ms),
          selfPeerId_(std::move(selfPeerId)),
          choked_(true),
          terminated_(false),
          pieceStorage_(pieceStorage),
          pendingBlock_(false),
          failed_(false) {}

void PeerConnect::Run() {
    while (!terminated_) {
        if (EstablishConnection()) {
            std::cout << "Connection established to peer" << std::endl;
            try {
                MainLoop();
            } catch (const std::exception &e) {
                Terminate();
            }
        } else {
            std::cerr << "Cannot establish connection to peer" << std::endl;
            Terminate();
        }
    }
}

void PeerConnect::PerformHandshake() {
    socket_.EstablishConnection();
    std::string pstr = "BitTorrent protocol";
    std::string handshake;
    handshake += (char) pstr.length();
    handshake += pstr;
    for (int i = 0; i < 8; ++i)
        handshake += '\0';
    handshake += tf_.infoHash;
    handshake += selfPeerId_;
    socket_.SendData(handshake);
    std::string response = socket_.ReceiveData(handshake.length());
    std::string infoHashFromPeer = response.substr(28, 20);
    if (infoHashFromPeer != tf_.infoHash)
        throw std::runtime_error("Info hashes don't same!");
}

bool PeerConnect::EstablishConnection() {
    try {
        PerformHandshake();
        ReceiveBitfield();
        SendInterested();
        return true;
    } catch (const std::exception &e) {
        failed_ = true;
        std::cerr << "Failed to establish connection with peer " << socket_.GetIp() << ":" <<
                  socket_.GetPort() << " -- " << e.what() << std::endl;
        return false;
    }
}

void PeerConnect::ReceiveBitfield() {
    const std::string response = socket_.ReceiveData();
    Message message = Message::Parse(response);
    if (message.id == MessageId::Unchoke)
        choked_ = false;
    else if (message.id != MessageId::BitField)
        throw std::runtime_error("Cannot receive bitfield!");
    piecesAvailability_ = PeerPiecesAvailability(message.payload);
}

void PeerConnect::SendInterested() {
    std::string interested = Message::Init(MessageId::Interested, "").ToString();
    socket_.SendData(interested);
}

void PeerConnect::Terminate() {
    std::cerr << "Terminate" << std::endl;
    failed_ = true;
    terminated_ = true;
}

bool PeerConnect::Failed() const {
    return failed_;
}

void PeerConnect::RequestPiece() {
    if (!pieceInProgress_)
        pieceInProgress_ = pieceStorage_.GetNextPieceToDownload();

    bool allBlocksRetrieved;
    while (pieceInProgress_ and ((allBlocksRetrieved = pieceInProgress_->AllBlocksRetrieved()) or
                                 !piecesAvailability_.IsPieceAvailable(pieceInProgress_->GetIndex()))) {
        if (allBlocksRetrieved)
            pieceStorage_.PieceProcessed(pieceInProgress_);
        pieceInProgress_ = pieceStorage_.GetNextPieceToDownload();
    }
    if (!pieceInProgress_) {
        Terminate();
        return;
    }
    auto block = pieceInProgress_->FirstMissingBlock();

    std::string msgPayload;
    msgPayload += IntToBytes(block->piece, false);
    msgPayload += IntToBytes(block->offset, false);
    msgPayload += IntToBytes(block->length, false);
    socket_.SendData(Message::Init(MessageId::Request, msgPayload).ToString());
    pendingBlock_ = true;
}

void PeerConnect::MainLoop() {
    while (!terminated_) {
        Message msg = Message::Parse(socket_.ReceiveData());
        if (msg.id == MessageId::Choke)
            choked_ = true;
        else if (msg.id == MessageId::Unchoke)
            choked_ = false;
        else if (msg.id == MessageId::Piece) {
            pendingBlock_ = false;
            int blockOffset = BytesToInt(msg.payload.substr(4, 4));
            std::string blockData = msg.payload.substr(8);
            pieceInProgress_->SaveBlock(blockOffset, std::move(blockData));
        } else if (msg.id == MessageId::Have) {
            int pieceIndex = BytesToInt(msg.payload);
            piecesAvailability_.SetPieceAvailability(pieceIndex);
        }
        if (!choked_ && !pendingBlock_) {
            RequestPiece();
        }
    }
}

