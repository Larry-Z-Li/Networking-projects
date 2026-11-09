# miProxy

An HTTP proxy that sits between a browser and a video web server, and chooses the video quality for each chunk of an HLS stream based on the measured throughput.

## How it works

- **Connections:** each browser connection is handled in its own process, created with `fork()`. That process uses `select()` to relay data in both directions between the browser and the web server.
- **Available bitrates:** when the browser requests the video's manifest, the proxy reads the bitrates the video is available in.
- **Throughput:** for each video chunk, the proxy measures the throughput in bits per second (bytes received × 8 ÷ download time) and smooths it with an exponentially weighted moving average, `T_cur = α × T_new + (1 − α) × T_cur`, where α is `--adaptation-gain`. `T_cur` starts at the lowest available bitrate.
- **Choosing a bitrate:** for each chunk request, the proxy picks the highest bitrate `b` for which `T_cur ≥ b × --adaptation-bitrate-multiplier`, or the lowest bitrate if none qualifies, and rewrites the request to fetch that version of the chunk.
- **Name lookup:** if `--upstream-server-host` is a domain name instead of an IP address, the proxy resolves it once at startup by sending a DNS query to `--nameserver-ip` and `--nameserver-port`.

## Usage

```bash
make
./miProxy --proxy-host <ip> --proxy-port <port> \
          --upstream-server-host <ip-or-domain> --upstream-server-port <port> \
          --adaptation-gain <alpha> --adaptation-bitrate-multiplier <multiplier> \
          [--log-file-name <file>] \
          [--nameserver-ip <ip> --nameserver-port <port>]
```

The topology scripts in `../grader/` start the proxy with an adaptation gain of 0.35 and a bitrate multiplier of 1.5.

## Log format

One line per video chunk:

```
<browser-ip> <chunk-name> <server-ip> <download-seconds> <throughput-Kbps> <smoothed-throughput-Kbps> <bitrate-Kbps>
```
