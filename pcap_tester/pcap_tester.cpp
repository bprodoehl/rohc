#include <iostream>
#include <rohc/compressor.h>
#include <rohc/decomp.h>
#include <pcap.h>
#include <map>
#include "../src/network.h"

struct Host {
    Host()
    : comp(15000, ROHC::REORDERING_NONE, ROHC::IP_ID_BEHAVIOUR_RANDOM)
    , decomp(true, &comp)
    {
        
    }
    
    
    ROHC::Compressor comp;
    ROHC::Decompressor decomp;
};

typedef std::map<uint32_t, Host*> hosts_t;
typedef hosts_t::iterator h_iter;

hosts_t hosts;

Host* getHost(uint32_t addr) {
    if (!hosts.count(addr)) {
        hosts[addr] = new Host;
    }
    
    return hosts[addr];
}

size_t ipHeaderOffset = 14;

std::string currentFile;

void onPacket(u_char* user, const struct pcap_pkthdr* h, const u_char* bytes) {
    static int cnt = 0;
    
#if 0
    for (int i = 0; i < 30; ++i) {
        std::cout << std::hex << (unsigned) bytes[i] << " " << std::dec;
    }

    std::cout << "\n";
    std::cout << "i = " << cnt++ << "\n";
#endif
    const ROHC::iphdr* ip = reinterpret_cast<const ROHC::iphdr*>(bytes+ipHeaderOffset);
    
    Host* shost = getHost(ip->saddr);
    Host* dhost = getHost(ip->daddr);
    
    size_t inputSize = h->caplen - ipHeaderOffset;
    const u_char* ipStart = bytes + ipHeaderOffset;
    ROHC::data_t input(ipStart, ipStart + inputSize);
    ROHC::data_t compressed;
    shost->comp.compress(input, compressed);
    ROHC::data_t decompressed;
    dhost->decomp.Decompress(compressed, decompressed);

    const ROHC::iphdr* ipo = reinterpret_cast<const ROHC::iphdr*>(decompressed.data());
    const ROHC::udphdr* udp = reinterpret_cast<const ROHC::udphdr*>(ipStart + ip->ihl * 4);
    ROHC::udphdr* udpo = reinterpret_cast<ROHC::udphdr*>(decompressed.data() + ipo->ihl * 4);

    if (input != decompressed) {
        std::cout << "UDP size in " << htons(udp->len) << " out " << htons(udpo->len) << "\n";
        if (input.size() == decompressed.size()) {
            for (size_t i = 0; i < input.size(); ++i) {
                if (input[i] != decompressed[i]) {
                    std::cout << "diff at " << i << "\n";
                }
            }
        }
        
        std::cerr << "Failed: " << currentFile << "\n";
        exit(1);
    }
    
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Please specify a pcap file" << std::endl;
        return 1;
    }
    currentFile = argv[1];
    
    //std::cout << "loading " << argv[1] << "\n";
    
    char errbuf[PCAP_ERRBUF_SIZE];

    pcap_t* pcap = pcap_open_offline(argv[1], errbuf);
    if (!pcap) {
        std::cerr << errbuf << std::endl;
        return 1;
    }
    
    //int dataLink = pcap_datalink(pcap);
    //std::cout << "data link is " << dataLink << "\n";
    
    pcap_loop(pcap, -1, onPacket, 0);
    
    pcap_close(pcap);
}