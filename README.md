# FHE Blockchain: Proof-of-Useful-Work with Fully Homomorphic Encryption

A **proof-of-concept** blockchain implementation that replaces traditional Proof-of-Work (PoW) with a **Proof-of-Useful-Work (PoUW)** mechanism based on **Fully Homomorphic Encryption (FHE)** computations verified by **zero-knowledge proofs (zkSNARKs)**.

> **Note**: This is an experimental research prototype demonstrating the feasibility of the protocol. It is not intended for production use.

## Overview

This project implements a novel consensus protocol where miners perform useful computations on encrypted data instead of solving arbitrary hash puzzles. The key innovation is that:

1. Users can submit **computations on encrypted data** to the network
2. Miners evaluate these FHE computations to mine blocks
3. Correctness is verified using **transparent zkSNARKs** (Aurora)
4. Privacy is preserved - miners never see the plaintext data

This approach transforms the "wasted" computational effort of traditional PoW into privacy-preserving computation services.

## Theoretical Background

### Fully Homomorphic Encryption (FHE)

FHE allows computations to be performed directly on encrypted data without decryption:

```
E(m1) ⋆ E(m2) = E(m1 ⋆ m2)
```

This implementation uses the **BGV (Brakerski-Gentry-Vaikuntanathan)** scheme, a leveled FHE scheme based on the Ring Learning With Errors (RLWE) problem. Key characteristics:

- **Leveled FHE**: Supports circuits up to a predetermined multiplicative depth
- **Modulus switching**: Controls noise growth between operations
- **Key switching**: Manages ciphertext dimension after multiplications
- Additions are essentially "free" while multiplications consume levels

The difficulty of a computation is measured by its **multiplicative depth** - the number of sequential multiplications required.

### Expression Tree Balancing

Since multiplicative depth directly impacts both computation cost and the difficulty metric, minimizing depth for a given expression is important. The implementation parses arithmetic expressions using the **Shunting Yard algorithm** and constructs an **Abstract Syntax Tree (AST)**.

The AST is then **balanced through tree rotations** to minimize the critical path of multiplications. For example, a left-skewed expression like `((a * b) * c) * d` (depth 3) can be restructured to `(a * b) * (c * d)` (depth 2) without changing the result. This optimization:

- Reduces the number of expensive FHE maintenance operations (key/modulus switching)
- Allows more efficient use of the leveled FHE scheme's capacity
- Ensures fair difficulty accounting regardless of how users write expressions

### Zero-Knowledge Proofs (zkSNARKs)

To verify that computations were performed correctly without re-executing them, the protocol uses **Aurora** - a transparent zkSNARK for R1CS (Rank-1 Constraint System) satisfiability.

Aurora properties that make it suitable:
- **Transparent**: No trusted setup required (critical for trustless blockchains)
- **Succinct**: Proof size is O(log²n) for n constraints
- **Efficient verification**: O(n) verification time

The FHE computation is expressed as an arithmetic circuit, converted to R1CS constraints, and proven correct using Aurora.

### Blockchain Consensus

The protocol follows Bitcoin's UTXO model with modifications:

- **Blocks** contain transactions and FHE computations with proofs
- **Mining** requires evaluating FHE computations and generating validity proofs
- **Validation** verifies proofs instead of checking hash targets
- **Difficulty** is the total multiplicative depth of computations in a block

### Binding Computations to the Chain

A critical innovation is the **computation binding** technique that links proofs to specific block data:

1. **Deterministic encryptions of zero** are added to input ciphertexts
2. The encryption randomness is derived from a PRG seeded with block data:
   - Previous block hash
   - Merkle root
   - Timestamp
   - Difficulty target
   - Computation descriptions and indices

This ensures:
- Tampering with any block invalidates all subsequent computation proofs
- Precomputation attacks are infeasible (proofs can't be reused)
- The chain maintains integrity similar to traditional PoW

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         Node                                 │
├─────────────┬─────────────┬─────────────┬──────────────────┤
│   Chain     │   Network   │   Storage   │     Wallet       │
│  Manager    │   Manager   │   Layer     │                  │
├─────────────┼─────────────┼─────────────┼──────────────────┤
│ - Main chain│ - P2P peers │ - Blocks    │ - Key pairs      │
│ - Forks     │ - RPC server│ - Chainstate│ - UTXO tracking  │
│ - Miner     │ - Router    │ - Mempool   │ - TX creation    │
│             │             │ - CompStore │                  │
└─────────────┴─────────────┴─────────────┴──────────────────┘
                              │
              ┌───────────────┴───────────────┐
              │        FHE Computer           │
              ├───────────────────────────────┤
              │ - Expression parsing (AST)    │
              │ - Homomorphic evaluation      │
              │ - Constraint generation       │
              │ - Aurora proof generation     │
              └───────────────────────────────┘
```

### Key Components

| Component | Purpose |
|-----------|---------|
| `FHEComputer` | Evaluates FHE computations and generates proofs |
| `ChainManager` | Manages blockchain state, forks, and mining |
| `ConnectionManager` | P2P networking with async I/O |
| `Wallet` | Ed25519 key management and transaction creation |
| `Node` | Orchestrates all components and message handling |

## Dependencies

- **OpenFHE** (zkOpenFHE fork) - FHE operations
- **libIOP** - Interactive Oracle Proofs
- **libsnark** - zk-SNARK primitives
- **libsodium** - Ed25519 signatures
- **Protocol Buffers** - Message serialization
- **ASIO** - Async networking

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Executables

| Binary | Description |
|--------|-------------|
| `conn` | Main blockchain node |
| `main` | FHE computation evaluator (standalone) |
| `gen_comp` | Generate sample computations |
| `gen_keys` | Generate FHE key pairs |
| `decryptor` | Decrypt FHE ciphertexts |

## Configuration

Edit `config/config.json`:

```json
{
  "net": {
    "address": "127.0.0.1",
    "port": 10725,
    "rpc_port": 11725,
    "bootstrap": [["127.0.0.1", 10726]],
    "outbound_peers_limit": 8,
    "inbound_peers_limit": 128
  },
  "chain": {
    "genesis": {
      "public_key": "<base64>",
      "timestamp": 1234567890,
      "reward": 5000000000,
      "difficulty": 1
    },
    "blocks_per_epoch": 2016,
    "seconds_per_block": 600
  }
}
```

## Computation Format

Users submit computations as JSON:

```json
{
  "expression": "(0 + 1) * (2 + 3)",
  "ciphertexts": ["<serialized_ct0>", "<serialized_ct1>", ...],
  "public_key": "<serialized_pk>",
  "eval_mult_key": "<serialized_evk>"
}
```

**Supported operations:**
- Addition: `+`
- Subtraction: `-`
- Multiplication: `*`

Operands are indices into the ciphertext array.

## Protocol Flow

### Mining a Block

1. Collect transactions from mempool
2. Select computations from computation pool (total difficulty >= target)
3. For each computation:
   - Bind to block data (add deterministic zero encryptions)
   - Evaluate FHE circuit
   - Generate Aurora proof
4. Broadcast block with proofs

### Validating a Block

1. Verify block links to current chain tip
2. Validate all transactions (signatures, no double-spends)
3. Check Merkle root
4. For each computation:
   - Reconstruct binding from block data
   - Verify Aurora proof
5. Confirm total difficulty meets threshold

## Security Properties

- **Precomputation resistance**: Proofs are bound to block data; can't be computed in advance
- **Tamper evidence**: Modifying any block invalidates all subsequent proofs
- **Privacy**: Miners compute on encrypted data without learning plaintexts
- **Transparency**: No trusted setup required (unlike many zkSNARK systems)
- **Double-spend prevention**: Standard UTXO model with signature verification

## Related Work

This protocol builds on research in Proof-of-Useful-Work:
- Ofelimos (CRYPTO 2022) - Combinatorial optimization PoUW
- NooShare - Monte Carlo computations
- Primecoin - Prime number discovery

Key differentiator: This is the first PoUW mechanism supporting **general, user-submitted, privacy-preserving computations**.

## Open Problems

- **Difficulty estimation**: Multiplicative depth is a simplification. A robust metric should account for proof generation cost, circuit width, and verification asymmetry.

- **Verification complexity**: Aurora has O(n) verification (succinctness refers to proof size, not verifier time). While cheaper than re-executing FHE, this may not scale well. Transparent SNARKs with sublinear verification are an active research area - the protocol will improve as better proof systems emerge.

- **Block storage**: Full computations (multi-MB ciphertexts) are stored in blocks. A production system should use a **computation pool** where only a UID references the computation, with full data fetched separately.

- **Security proofs**: The protocol needs formal analysis of consensus guarantees, binding security, and incentive compatibility.

- **Other**: Fee structures, proof aggregation, alternative FHE schemes (CKKS, TFHE), hardware acceleration.

## References

- Brakerski, Gentry, Vaikuntanathan - "(Leveled) Fully Homomorphic Encryption Without Bootstrapping" (2012)
- Ben-Sasson et al. - "Aurora: Transparent Succinct Arguments for R1CS" (EUROCRYPT 2019)
- Viand, Knabenhans, Hithnawi - "Verifiable Fully Homomorphic Encryption" (2023)
- Nakamoto - "Bitcoin: A Peer-to-Peer Electronic Cash System" (2009)
