Video Streaming via CDN Project
This repository contains an implementation of a simplified video Content Delivery Network (CDN) that demonstrates two core techniques used by real-world streaming services: adaptive bitrate selection and DNS-based server load balancing.
The system runs entirely inside Mininet to simulate multiple hosts, custom IP addresses, and controllable network conditions (bandwidth and latency).
What It Does

miProxy — a custom HTTP proxy written in C/C++ that intercepts HLS video requests from a browser, measures real-time throughput to the upstream server, smooths it using an exponentially weighted moving average (EWMA), and dynamically rewrites requests to fetch the highest-quality video chunks the connection can reliably support. It supports concurrent browser connections and logs detailed per-segment statistics.
nameserver — a minimal authoritative DNS server that resolves a single domain (video.cdn.assignment2.test) to one of several content server IPs. It operates in two modes:
Round-robin: cycles through a static list of server IPs
Geographic/lowest-cost: computes the shortest path through a given network topology file and returns the closest server to the requesting client’s IP


Together, these components allow a standard web browser (e.g. Chromium) to stream HLS video at the best possible quality from the most suitable server, all while running in a controlled Mininet environment.
Key Features

Adaptive bitrate logic moved from the player into the proxy
Per-segment throughput measurement and EWMA smoothing
HLS manifest parsing and URI rewriting for bitrate switching
Concurrent connection handling with select()
Simplified DNS wire-format parsing and response generation
Strict logging formats for both proxy and nameserver (used by the autograder)

Directory Structure

miProxy/ — source code, Makefile, and binary for the HTTP proxy
nameserver/ — source code, Makefile, and binary for the DNS server
grader/ — provided autograder (do not modify)
.images/ — diagrams referenced in documentation

How to Run (Quick Start)

Build both components:Bashcd miProxy && make
cd ../nameserver && make
Launch a simple single-server topology:Bashpython3 startTopologyMininet.pyThen open http://10.0.2.100:9000 in Chromium.
Launch the multi-server + DNS topology:Bashpython3 startTopologyMininetMultiHost.pyTry http://10.0.4.100:9000 or http://10.0.5.100:9000.
Run the autograder to verify:Bashcd grader
sudo python3 grader.py --all

Enjoy exploring how video CDNs deliver smooth, high-quality streaming at scale!1.6sFastremove the grader partHere’s the updated README.md with all references to the grader, autograding, and grading commands removed. It now reads purely as a project showcase/description.
Markdown# Video Streaming via CDN Project

This repository contains an implementation of a simplified video Content Delivery Network (CDN) that demonstrates two core techniques used by real-world streaming services: adaptive bitrate selection and DNS-based server load balancing.

The system runs entirely inside Mininet to simulate multiple hosts, custom IP addresses, and controllable network conditions (bandwidth and latency).

## What It Does

- **miProxy** — a custom HTTP proxy written in C/C++ that intercepts HLS video requests from a browser, measures real-time throughput to the upstream server, smooths it using an exponentially weighted moving average (EWMA), and dynamically rewrites requests to fetch the highest-quality video chunks the connection can reliably support. It supports concurrent browser connections and logs detailed per-segment statistics.

- **nameserver** — a minimal authoritative DNS server that resolves a single domain (`video.cdn.assignment2.test`) to one of several content server IPs. It operates in two modes:
  - Round-robin: cycles through a static list of server IPs
  - Geographic/lowest-cost: computes the shortest path through a given network topology file and returns the closest server to the requesting client’s IP

Together, these components allow a standard web browser (e.g. Chromium) to stream HLS video at the best possible quality from the most suitable server, all while running in a controlled Mininet environment.

## Key Features

- Adaptive bitrate logic moved from the player into the proxy
- Per-segment throughput measurement and EWMA smoothing
- HLS manifest parsing and URI rewriting for bitrate switching
- Concurrent connection handling with `select()`
- Simplified DNS wire-format parsing and response generation
- Strict logging formats for both proxy and nameserver

## Directory Structure

- `miProxy/` — source code, Makefile, and binary for the HTTP proxy
- `nameserver/` — source code, Makefile, and binary for the DNS server
- `.images/` — diagrams referenced in documentation

## How to Run (Quick Start)

1. Build both components:
   ```bash
   cd miProxy && make
   cd ../nameserver && make

Launch a simple single-server topology:Bashpython3 startTopologyMininet.pyThen open http://10.0.2.100:9000 in Chromium.
Launch the multi-server + DNS topology:Bashpython3 startTopologyMininetMultiHost.pyTry http://10.0.4.100:9000 or http://10.0.5.100:9000.

Enjoy exploring how video CDNs deliver smooth, high-quality streaming at scale!
