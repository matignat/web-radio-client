# Internet Radio Client - sikradio

A C++ implementation of an Internet radio client communicating with a remote server over **TCP** and supporting both **IPv4 and IPv6**.

The project implements the communication protocol from scratch using the **socket API**, handles continuous audio streaming, reconnection, multiplexed text data, configurable timeouts, and multiple runtime options.

## Features

* TCP-based communication
* IPv4 and IPv6 support
* Automatic IP version selection
* Manual IPv4 / IPv6 selection
* Continuous audio stream reception
* Transparent forwarding of received audio data to standard output
* Optional multiplexing of text information
* Automatic reconnection after a timeout
* Configurable connection timeout
* Graceful connection termination
* Interactive `quit` command
* Configurable diagnostic output
* Robust command-line argument parsing
* Support for both short and grouped parameters

## Usage

The client is launched using:

```bash
./sikradio -u <url>
```

### Available options

| Option         | Description                                                    |
| -------------- | -------------------------------------------------------------- |
| `-u url`       | Server and audio stream identifier                             |
| `-m`           | Request multiplexing of text information with the audio stream |
| `-t timeout`   | Connection timeout in milliseconds                             |
| `-4`           | Force IPv4                                                     |
| `-6`           | Force IPv6                                                     |
| `-v verbosity` | Set diagnostic output level (`0–4`)                            |
| `-q`           | Equivalent to `-v0`                                            |

For example:

```bash
./sikradio -u <url> -m -t 5000 -4
```

The client also supports grouped flags, for example:

```bash
./sikradio -m46
```

## Audio Playback

The client does not decode the received audio stream itself. Instead, the raw stream is written directly to standard output and can be piped to an external audio player.

For example:

```bash
./sikradio -u <url> | play -q -t mp3 -
```

Alternatively, `mpv` can be used:

```bash
./sikradio -u <url> | mpv --really-quiet -
```

This approach allows the client to focus on network communication and stream handling while leaving audio decoding and playback to a dedicated external program.

## Diagnostic Output

The verbosity level controls the amount of information printed to standard error:

| Level | Information                                     |
| ----- | ----------------------------------------------- |
| `0`   | No additional diagnostic information            |
| `1`   | Information about communication with the server |
| `2`   | Critical errors preventing further operation    |
| `3`   | Non-critical system or library errors           |

The `-q` option is a shortcut for `-v0`.

## Connection Handling

The client supports several connection-related scenarios:

* If the server closes the connection, all received data is preserved and the client exits successfully.
* If the connection times out, the client disconnects and attempts to reconnect.
* If the user enters `quit`, the client closes the connection, outputs all received data, and terminates successfully.
* Critical errors result in a non-zero exit status.
* Invalid command-line arguments are reported and result in a non-zero exit status.

## Implementation

The project was implemented in **C++** using the system **socket API**.

The implementation focuses on:

* TCP socket communication
* Address resolution with `getaddrinfo`
* IPv4 / IPv6 compatibility
* Connection management
* Receiving and forwarding continuous data streams
* Timeout handling
* Reconnection logic
* Command-line argument parsing
* Error handling
* Multiplexed data processing

No external networking libraries are used for the communication layer.

## Building

The project includes a `Makefile`.

Build the client with:

```bash
make
```

This produces the executable:

```text
sikradio
```

To remove generated build files:

```bash
make clean
```

## Project Context

This project was developed as part of a university networking assignment focused on practical network programming and socket-based communication.

The assignment required reverse-engineering the communication protocol from provided examples and implementing a reliable streaming client capable of operating in both IPv4 and IPv6 environments.

The implementation was written from scratch in **C++**, with particular emphasis on correct stream handling, robust error management, and uninterrupted audio playback.
