#include <cstdio>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <random>
#include <thread>
#include "torrent_tracker.h"
#include "piece_storage.h"
#include "peer_connect.h"

std::string RandomString(size_t length) {
    std::random_device random;
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(random() % ('Z' - 'A') + 'A');
    }
    return result;
}

const std::string PeerId = "SOMERANDOMPEERID" + RandomString(4);
std::filesystem::path downloadDirectory, torrentFilePath;
int percents = 2;
//size_t PiecesToDownload = 20;

bool RunDownloadMultithread(PieceStorage& pieces, const TorrentFile& torrentFile, const std::string& ourId, const TorrentTracker& tracker) {
    using namespace std::chrono_literals;

    std::vector<std::thread> peerThreads;
    std::vector<PeerConnect> peerConnections;

    for (const Peer& peer : tracker.GetPeers()) {
        peerConnections.emplace_back(peer, torrentFile, ourId, pieces);
    }

    for (PeerConnect& peerConnect : peerConnections) {
        peerThreads.emplace_back(
                [&peerConnect] () {
                    bool tryAgain = true;
                    int attempts = 0;
                    do {
                        try {
                            ++attempts;
                            peerConnect.Run();
                        } catch (const std::runtime_error& e) {
                            std::cerr << "Runtime error: " << e.what() << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << "Exception: " << e.what() << std::endl;
                        } catch (...) {
                            std::cerr << "Unknown error" << std::endl;
                        }
                        tryAgain = peerConnect.Failed() && attempts < 3;
                    } while (tryAgain);
                }
        );
    }
    std::cout << "befor sleeping\n";
    std::this_thread::sleep_for(10s);
    std::cout << "after sleeping\n";
    while (pieces.PiecesSavedToDiscCount() < (pieces.TotalPiecesCount() * percents + 99) / 100) {
        if (pieces.PiecesInProgressCount() == 0) {
            std::cerr << "Want to download more pieces but all peer connections are not working. Let's request new peers" << std::endl;

            for (PeerConnect& peerConnect : peerConnections) {
                peerConnect.Terminate();
            }
            for (std::thread& thread : peerThreads) {
                thread.join();
            }
            return true;
        }
        std::this_thread::sleep_for(1s);
    }

    for (PeerConnect& peerConnect : peerConnections) {
        peerConnect.Terminate();
    }

    for (std::thread& thread : peerThreads) {
        thread.detach();
    }

    return false;
}

void DownloadTorrentFile(const TorrentFile& torrentFile, PieceStorage& pieces, const std::string& ourId) {
    std::cout << "Connecting to tracker " << torrentFile.announce << std::endl;

    TorrentTracker tracker(torrentFile.announce);
    bool requestMorePeers = false;
    do {
        tracker.UpdatePeers(torrentFile, ourId, 12345);

        if (tracker.GetPeers().empty()) {
            std::cerr << "No peers found. Cannot download a file" << std::endl;
        }

        std::cout << "Found " << tracker.GetPeers().size() << " peers" << std::endl;
        for (const Peer& peer : tracker.GetPeers()) {
            std::cout << "Found peer " << peer.ip << ":" << peer.port << std::endl;
        }

        requestMorePeers = RunDownloadMultithread(pieces, torrentFile, ourId, tracker);
    } while (requestMorePeers);
}

int main(int argc, char **argv)
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0)
            downloadDirectory = argv[i + 1];
        else if (strcmp(argv[i], "-p") == 0) {
            percents = std::stoi(argv[i + 1]);
            torrentFilePath = argv[i + 2];
        }
    }

    std::cout << downloadDirectory << "\n";
    std::cout << percents << "\n";
    std::cout << torrentFilePath << "\n";

    TorrentFile torrentFile;
    try {
        torrentFile = LoadTorrentFile(torrentFilePath);
        std::cout << "Loaded torrent file " << torrentFilePath << ". " << torrentFile.comment << std::endl;
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << std::endl;
    }

    PieceStorage pieces(torrentFile, downloadDirectory);

    DownloadTorrentFile(torrentFile, pieces, PeerId);

    return 0;
}