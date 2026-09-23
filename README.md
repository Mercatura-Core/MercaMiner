# MercaMiner

MercaMiner is the reference CPU miner for Mercatura (MCA).

It mines Mercatura's MercaHash V1 proof-of-work directly against Mercatura
Core using the Bitcoin-family `getblocktemplate` and `submitblock` RPC
interfaces.

MercaMiner v0.1 targets Linux x86_64 and solo RPC mining.

GPU mining and pool/Stratum protocols are outside the scope of v0.1.

## Step-by-step mining guide

For complete setup and operating instructions, see:

[MercaMiner v0.1 Step-by-Step Running Guide](doc/MercaMiner_v0.1_Running_Guide.docx)

## Status

MercaMiner v0.1 implements the standalone Mercatura CPU-mining path,
including:

- MercaHash V1 proof-of-work;
- Mercatura block-header serialization;
- compact-target decoding and proof-of-work comparison;
- `getblocktemplate` and `submitblock` RPC integration;
- Mercatura coinbase construction;
- witness-commitment construction;
- Mercatura payout-address handling;
- multi-threaded CPU mining;
- continuous mining;
- stale-work classification;
- network and genesis identity validation;
- proof-of-work limit validation;
- automatic local RPC configuration;
- RPC cookie-rotation recovery;
- configurable runtime status reporting;
- MercaHash benchmark tooling.

MercaMiner has completed local regtest and testnet validation.

Longer multi-node burn-in will continue as part of normal Mercatura testnet
operation.

## Requirements

MercaMiner currently targets Linux x86_64.

Build requirements:

- CMake 3.22 or newer;
- a C++20 compiler;
- libcurl development files;
- nlohmann-json 3.10.5 or newer.

On Ubuntu or Debian:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  libcurl4-openssl-dev \
  nlohmann-json3-dev
