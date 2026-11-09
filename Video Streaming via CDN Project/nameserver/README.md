# nameserver

A minimal authoritative DNS server that answers type A queries for a single domain with the IP address of one of several web servers.

## Modes

- **Round robin** (`--round-robin-ip-list-file-path`): the server cycles through the IP addresses in the given file, one per line, returning the next one for each query.
- **Geographic** (`--network-topology-file-path`): the server reads a network topology made of clients, switches and servers, with a cost on each link. For each query, it runs Dijkstra's algorithm from the client that asked and returns the closest server. See `../grader/resources/sample_network_topology.txt` for the file format.

For a query about another domain or another record type, or, in geographic mode, a query from a client that isn't in the topology, the server replies with a name error (`NXDOMAIN`).

## Usage

```bash
make
./nameserver --ip <ip> --port <port> --domain <domain> --log-file-name <file> \
             --round-robin-ip-list-file-path <file>
# or, for geographic mode:
./nameserver --ip <ip> --port <port> --domain <domain> --log-file-name <file> \
             --network-topology-file-path <file>
```

## Log format

Each answered query is logged as one line: `<client-ip> <queried-domain> <returned-ip>`. The log is written to `nameserver_log.txt`, regardless of the `--log-file-name` value.
