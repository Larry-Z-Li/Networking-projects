# Reliable Transport over UDP (WTP)

A sender (`wSender`) and a receiver (`wReceiver`) that transfer a file reliably over UDP, even when the network loses, delays, duplicates, reorders or corrupts packets. UDP on its own guarantees none of that, so these programs add the reliability that TCP normally provides. They are written in C++ with POSIX sockets, for a course assignment whose full specification is in [`ASSIGNMENT.md`](ASSIGNMENT.md).

## How it works

- **Packet format.** Every packet starts with a 16-byte header holding four fields: `type`, `seqNum`, `length` and `checksum`. Up to 1,456 bytes of data follow, so each packet fits in the 1,472 bytes that remain in a 1,500-byte Ethernet frame after the IP and UDP headers. There are four packet types: `START`, `END`, `DATA` and `ACK`.
- **Connection.** The sender sends `START` and waits for its `ACK`. It then sends the file as `DATA` packets numbered from 0, and finishes with `END`.
- **Sliding window.** The sender keeps up to `window-size` unacknowledged packets in flight at once.
- **Checksums.** Each packet carries a CRC32 checksum of its data, and the receiver drops any packet whose checksum doesn't match.
- **Cumulative ACKs.** The receiver acknowledges with the next sequence number it expects. It buffers packets that arrive out of order, as long as they fall inside the window, and drops packets beyond the window.
- **Retransmission.** If the window hasn't moved forward for 400 ms, the sender resends every packet in the window.
- **Output and logs.** The receiver writes each received file to the output directory as `FILE-<n>.out`. Both programs log the header of every packet they send and receive.

## Usage

```bash
cd WTP-base
make
./wReceiver <port> <window-size> <output-dir> <log-file>
./wSender <receiver-ip> <receiver-port> <window-size> <input-file> <log-file>
```

The receiver adds the file name directly after `<output-dir>`, so end it with a `/`.

## Directory structure

| Folder or file | Contents |
| --- | --- |
| `WTP-base/` | `wSender.cpp`, `wReceiver.cpp` and the Makefile |
| `starter_files/` | `PacketHeader.h` and `crc32.h`, provided by the course |
| `grader/` | Test scripts provided by the course, including a UDP proxy that can corrupt packets between the sender and the receiver |
| `ASSIGNMENT.md` | The original assignment specification |
