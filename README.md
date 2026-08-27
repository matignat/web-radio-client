# sikradio – Internet Radio Client

## Overview

`sikradio` is a simple internet radio client implemented in C++, using TCP sockets over IPv4 and IPv6.

It connects to HTTP or HTTPS streaming servers, follows HTTP redirects, and forwards the received audio stream unchanged to standard output, where it can be piped to an external player such as `play` or `mpv`.

The client optionally requests and handles ICY metadata (for example current song titles) multiplexed into the audio stream and prints this textual information to standard error.

## Building

The program is intended to be built on Linux with a C++ compiler and OpenSSL libraries available.

Makefile included

## Usage

The client is run with the following syntax:

```bash
sikradio -u url [-m] [-t timeout] [-4] [-6] [-v verbosity] [-q]
```

## Options

- `-u url` – URL of the server and audio stream, required.
- `-m` – request ICY metadata to be multiplexed with the audio stream.
- `-t timeout` – timeout in milliseconds for receiving data, range `100–100000`, default `5000`.
- `-4` – force IPv4 only.
- `-6` – force IPv6 only.
- `-v verbosity` – diagnostic output level, range `0–4`, default `2`.
- `-q` – shortcut for `-v0`.

Options may be provided in any order and may also be grouped, for example `-m46`.

If both `-4` and `-6` are given, or neither is given, the client uses the address family of the first address returned by `getaddrinfo`.

If an option is repeated, the last value takes precedence.

## Verbosity levels

- `0` – no additional diagnostic output
- `1` – communication progress information
- `2` – critical errors preventing further execution
- `3` – non-critical errors

## Examples

Basic usage with `play`:

```bash
sikradio -u http://stream.radiobaobab.pl:8000/radiobaobab.mp3 \
  | play -q -t mp3 -
```

Requesting ICY metadata and setting a custom timeout:

```bash
sikradio -u https://stream.nowyswiat.online/mp3 -m -t 3500 \
  | play -q -t mp3 -
```

Using `mpv` instead of `play`:

```bash
sikradio -u http://stream3.polskieradio.pl:8904 \
  | mpv --really-quiet -
```

## Features

- Supports both HTTP and HTTPS streams
- Supports IPv4 and IPv6 connections
- Parses and validates HTTP/HTTPS URLs
- Follows HTTP redirects
- Detects redirect loops
- Extracts and forwards cookies during redirects
- Streams audio data directly to standard output
- Supports ICY metadata streaming
- Prints metadata to standard error
- Supports timeout-based reconnecting
- Allows graceful shutdown when the user types `quit`

## Program behavior

The client does not decode or interpret the audio stream. It writes the received audio data exactly as received to standard output.

When metadata mode is enabled with `-m` and the server provides the `icy-metaint` header, the client reads metadata blocks from the stream and writes them to standard error.

The client monitors both the socket and standard input. If the user types `quit` and presses Enter, the program closes the connection and exits normally.

If no data is received within the configured timeout, the client closes the connection and retries automatically.

## Exit status

- `0` – normal termination, for example when the server closes the connection or the user types `quit`
- `1` – invalid arguments or a critical runtime error

## Implementation notes

The project is implemented in C++ using POSIX sockets and `poll()` for communication, timeout handling, and quit detection.

HTTPS support is implemented using OpenSSL (`libssl` and `libcrypto`).

The client includes:
- command-line argument parsing and validation
- URL parsing with support for IPv6 and explicit ports
- HTTP response parsing
- header lookup and redirect handling
- cookie extraction
- plain TCP audio streaming
- TLS-protected audio streaming
- ICY metadata handling for both plain and TLS connections

## Limitations

- Only `http://` and `https://` URLs are supported
- Only basic redirect formats are supported
- Audio data is forwarded unchanged and is not decoded by the client itself
- Metadata support depends on server-side ICY support

## Summary

`sikradio` is a lightweight command-line internet radio client designed for streaming audio over HTTP and HTTPS, with optional ICY metadata support, redirect handling, timeout recovery, and compatibility with external audio players.
