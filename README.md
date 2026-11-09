# Networking Projects

Two computer networking course projects, written in C++ with POSIX sockets.

| Project | What it does |
| --- | --- |
| [Reliable Transport over UDP (WTP)](Reliable%20Transport%20Protocol%20%28WTP%29%20over%20UDP/) | A sender and receiver that transfer files reliably over UDP despite loss, delay, duplication, reordering and corruption, using a sliding window, cumulative ACKs, CRC32 checksums and timeout-based retransmission |
| [Video Streaming via CDN](Video%20Streaming%20via%20CDN%20Project/) | A simplified video CDN that runs in Mininet: an HTTP proxy that adapts the video bitrate to the measured throughput, and a DNS server that balances clients across web servers by round robin or by shortest network path |

Each project's README explains its design and how to build and run it.
