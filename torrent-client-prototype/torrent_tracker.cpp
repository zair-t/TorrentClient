#include "torrent_tracker.h"
#include "bencode.h"
#include "byte_tools.h"
#include <cpr/cpr.h>

#include <utility>

TorrentTracker::TorrentTracker(std::string url)
        : url_(std::move(url)) {}

void TorrentTracker::UpdatePeers(const TorrentFile &tf, std::string peerId, int port) {
    cpr::Response response = cpr::Get(
            cpr::Url{url_},
            cpr::Parameters {
                    {"info_hash", tf.infoHash},
                    {"peer_id", peerId},
                    {"port", std::to_string(port)},
                    {"uploaded", std::to_string(0)},
                    {"downloaded", std::to_string(0)},
                    {"left", std::to_string(tf.length)},
                    {"compact", std::to_string(1)}
            },
            cpr::Timeout{20000}
    );

    if (response.status_code == 200) {
        std::string responseText = response.text;
        if (responseText.find("failure reason") == std::string::npos) {
            Bencode::Decoder decoder(responseText);
            auto dict = std::get<Bencode::dictionary>(decoder.DecodeElement().value);
            std::string_view peers(std::get<Bencode::string>(dict["peers"].value));

            size_t i = peers.size();
            while (i > 0) {
                unsigned int ip = BytesToInt(peers);

                int peerPort = static_cast<int>(static_cast<unsigned char>(peers[4]) << 8 |
                                                static_cast<unsigned char>(peers[5]));

                peers_.push_back({std::to_string(ip >> 24) + "." +
                                  std::to_string((ip << 8) >> 24) + "." +
                                  std::to_string((ip << 16) >> 24) + "." +
                                  std::to_string(((ip << 24) >> 24)), peerPort});
                peers.remove_prefix(6);
                i -= 6;
            }
        } else {
            std::cerr << "Server responded '" << responseText << "'" << std::endl;
        }
    }
}

const std::vector<Peer> &TorrentTracker::GetPeers() const {
    return peers_;
}
