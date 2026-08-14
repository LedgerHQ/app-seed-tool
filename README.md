<p align="center"><img src="icons/seed_tool.png" alt="Seed Tool" style="width:10%;height:10%"/></p>

# Seed Tool: A Ledger application that provides some useful seed management utilities

[![Release](https://img.shields.io/github/release/aido/app-seed-tool)](https://github.com/aido/app-seed-tool/releases)
[![License](https://img.shields.io/github/license/aido/app-seed-tool)](https://github.com/aido/app-seed-tool/blob/develop/LICENSE)

![nanos](https://img.shields.io/badge/Nano_S-working-green?logo=data:image/svg%2bxml;base64,PHN2ZyB3aWR0aD0iMTQ3IiBoZWlnaHQ9IjEyOCIgdmlld0JveD0iMCAwIDE0NyAxMjgiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTAgOTEuNjU0OFYxMjhINTUuMjkzVjExOS45NEg4LjA1NjMxVjkxLjY1NDhIMFpNMTM4Ljk0NCA5MS42NTQ4VjExOS45NEg5MS43MDdWMTI3Ljk5OEgxNDdWOTEuNjU0OEgxMzguOTQ0Wk01NS4zNzMzIDM2LjM0NTJWOTEuNjUyOUg5MS43MDdWODQuMzg0Mkg2My40Mjk2VjM2LjM0NTJINTUuMzczM1pNMCAwVjM2LjM0NTJIOC4wNTYzMVY4LjA1ODQ0SDU1LjI5M1YwSDBaTTkxLjcwNyAwVjguMDU4NDRIMTM4Ljk0NFYzNi4zNDUySDE0N1YwSDkxLjcwN1oiIGZpbGw9IndoaXRlIi8+PC9zdmc+)
![nanosp](https://img.shields.io/badge/Nano_S+-working-green?logo=data:image/svg%2bxml;base64,PHN2ZyB3aWR0aD0iMTQ3IiBoZWlnaHQ9IjEyOCIgdmlld0JveD0iMCAwIDE0NyAxMjgiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTAgOTEuNjU0OFYxMjhINTUuMjkzVjExOS45NEg4LjA1NjMxVjkxLjY1NDhIMFpNMTM4Ljk0NCA5MS42NTQ4VjExOS45NEg5MS43MDdWMTI3Ljk5OEgxNDdWOTEuNjU0OEgxMzguOTQ0Wk01NS4zNzMzIDM2LjM0NTJWOTEuNjUyOUg5MS43MDdWODQuMzg0Mkg2My40Mjk2VjM2LjM0NTJINTUuMzczM1pNMCAwVjM2LjM0NTJIOC4wNTYzMVY4LjA1ODQ0SDU1LjI5M1YwSDBaTTkxLjcwNyAwVjguMDU4NDRIMTM4Ljk0NFYzNi4zNDUySDE0N1YwSDkxLjcwN1oiIGZpbGw9IndoaXRlIi8+PC9zdmc+)
![nanox](https://img.shields.io/badge/Nano_X-working-green?logo=data:image/svg%2bxml;base64,PHN2ZyB3aWR0aD0iMTQ3IiBoZWlnaHQ9IjEyOCIgdmlld0JveD0iMCAwIDE0NyAxMjgiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTAgOTEuNjU0OFYxMjhINTUuMjkzVjExOS45NEg4LjA1NjMxVjkxLjY1NDhIMFpNMTM4Ljk0NCA5MS42NTQ4VjExOS45NEg5MS43MDdWMTI3Ljk5OEgxNDdWOTEuNjU0OEgxMzguOTQ0Wk01NS4zNzMzIDM2LjM0NTJWOTEuNjUyOUg5MS43MDdWODQuMzg0Mkg2My40Mjk2VjM2LjM0NTJINTUuMzczM1pNMCAwVjM2LjM0NTJIOC4wNTYzMVY4LjA1ODQ0SDU1LjI5M1YwSDBaTTkxLjcwNyAwVjguMDU4NDRIMTM4Ljk0NFYzNi4zNDUySDE0N1YwSDkxLjcwN1oiIGZpbGw9IndoaXRlIi8+PC9zdmc+)
![stax](https://img.shields.io/badge/Stax-working-green?logo=data:image/svg%2bxml;base64,PHN2ZyB3aWR0aD0iMTQ3IiBoZWlnaHQ9IjEyOCIgdmlld0JveD0iMCAwIDE0NyAxMjgiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTAgOTEuNjU0OFYxMjhINTUuMjkzVjExOS45NEg4LjA1NjMxVjkxLjY1NDhIMFpNMTM4Ljk0NCA5MS42NTQ4VjExOS45NEg5MS43MDdWMTI3Ljk5OEgxNDdWOTEuNjU0OEgxMzguOTQ0Wk01NS4zNzMzIDM2LjM0NTJWOTEuNjUyOUg5MS43MDdWODQuMzg0Mkg2My40Mjk2VjM2LjM0NTJINTUuMzczM1pNMCAwVjM2LjM0NTJIOC4wNTYzMVY4LjA1ODQ0SDU1LjI5M1YwSDBaTTkxLjcwNyAwVjguMDU4NDRIMTM4Ljk0NFYzNi4zNDUySDE0N1YwSDkxLjcwN1oiIGZpbGw9IndoaXRlIi8+PC9zdmc+)
![flex](https://img.shields.io/badge/Flex-working-green?logo=data:image/svg%2bxml;base64,PHN2ZyB3aWR0aD0iMTQ3IiBoZWlnaHQ9IjEyOCIgdmlld0JveD0iMCAwIDE0NyAxMjgiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTAgOTEuNjU0OFYxMjhINTUuMjkzVjExOS45NEg4LjA1NjMxVjkxLjY1NDhIMFpNMTM4Ljk0NCA5MS42NTQ4VjExOS45NEg5MS43MDdWMTI3Ljk5OEgxNDdWOTEuNjU0OEgxMzguOTQ0Wk01NS4zNzMzIDM2LjM0NTJWOTEuNjUyOUg5MS43MDdWODQuMzg0Mkg2My40Mjk2VjM2LjM0NTJINTUuMzczM1pNMCAwVjM2LjM0NTJIOC4wNTYzMVY4LjA1ODQ0SDU1LjI5M1YwSDBaTTkxLjcwNyAwVjguMDU4NDRIMTM4Ljk0NFYzNi4zNDUySDE0N1YwSDkxLjcwN1oiIGZpbGw9IndoaXRlIi8+PC9zdmc+)
![nano_gen5](https://img.shields.io/badge/Nano_Gen5-working-green?logo=data:image/svg%2bxml;base64,PHN2ZyB3aWR0aD0iMTQ3IiBoZWlnaHQ9IjEyOCIgdmlld0JveD0iMCAwIDE0NyAxMjgiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+PHBhdGggZD0iTTAgOTEuNjU0OFYxMjhINTUuMjkzVjExOS45NEg4LjA1NjMxVjkxLjY1NDhIMFpNMTM4Ljk0NCA5MS42NTQ4VjExOS45NEg5MS43MDdWMTI3Ljk5OEgxNDdWOTEuNjU0OEgxMzguOTQ0Wk01NS4zNzMzIDM2LjM0NTJWOTEuNjUyOUg5MS43MDdWODQuMzg0Mkg2My40Mjk2VjM2LjM0NTJINTUuMzczM1pNMCAwVjM2LjM0NTJIOC4wNTYzMVY4LjA1ODQ0SDU1LjI5M1YwSDBaTTkxLjcwNyAwVjguMDU4NDRIMTM4Ljk0NFYzNi4zNDUySDE0N1YwSDkxLjcwN1oiIGZpbGw9IndoaXRlIi8+PC9zdmc+)

[![Build app-seed-tool](https://github.com/aido/app-seed-tool/actions/workflows/ci-workflow.yml/badge.svg)](https://github.com/aido/app-seed-tool/actions/workflows/ci-workflow.yml)
[![CodeQL](https://github.com/aido/app-seed-tool/actions/workflows/codeql-workflow.yml/badge.svg)](https://github.com/aido/app-seed-tool/actions/workflows/codeql-workflow.yml)
[![Code style check](https://github.com/aido/app-seed-tool/actions/workflows/lint-workflow.yml/badge.svg)](https://github.com/aido/app-seed-tool/actions/workflows/lint-workflow.yml)
[![Ledger rule enforcer](https://github.com/aido/app-seed-tool/actions/workflows/ledger-rule-enforcer.yml/badge.svg)](https://github.com/aido/app-seed-tool/actions/workflows/ledger-rule-enforcer.yml)
[![codecov](https://codecov.io/gh/aido/app-seed-tool/branch/develop/graph/badge.svg?token=uCkGEbhGl3)](https://codecov.io/gh/aido/app-seed-tool/tree/develop)

---

Use the utilities provided by this Ledger application to check a backed-up [BIP-39](https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki) seed, generate [Shamir's Secret Sharing (SSS)](https://en.wikipedia.org/wiki/Shamir%27s_secret_sharing) shares for a seed, recover a [BIP-39](https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki) phrase from a Shamir's Secret Sharing backup, generate [BIP-85](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki) children, and more.

Not all Ledger devices are equal. The older, less capable devices do not have the capacity to provide a full range of seed utilities. The following table lists the seed utilities provided by each devices type:
<div align="center">

||Nano S|Nano S+|Nano X|Stax|Flex|Nano Gen5|
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
|[Check BIP-39](#check-bip-39)|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|
|[Check Shamir's secret shares](#check-shamirs-secret-shares)|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|
|[Generate Shamir's secret sharing](#generate-shamirs-secret-sharing)|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|
|[Recover BIP-39](#recover-bip-39)|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|
|[Generate BIP-85](#generate-bip-85)|$${\color{red}✗}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|

<table>
  <thead>
    <tr>
      <th colspan="2" align="center">Key</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td align="center">$${\color{green}✓}$$</td>
      <td>Supported</td>
    </tr>
    <tr>
      <td align="center">$${\color{orange}✓}$$</td>
      <td>Work in progress / Planned</td>
    </tr>
    <tr>
      <td align="center">$${\color{red}✗}$$</td>
      <td>Unsupported and with no plan to support</td>
    </tr>
  </tbody>
</table>
</div>

## Application menu flow

Seed Tool draws two interfaces, and which one a device gets follows its screen rather than its age: Stax, Flex and Nano Gen5 are driven by touch, Nano S, Nano S+ and Nano X by two buttons. The two do not offer the same utilities and do not arrange them the same way, so each has its own diagram below. Every box is a screen the application draws, named with the words that screen shows; every labelled arrow is the control that carries the flow on, or the outcome that chooses between them; and the diamond is the screen whose outcome the application decides rather than the user.

### Stax, Flex and Nano Gen5

The home page carries the version, the copyright and *Quit*, and its action button opens a menu of four intentions. Those four are the flows below.

```mermaid
---
title: Seed Tool menu flow - Stax, Flex and Nano Gen5
---
flowchart LR
    1 --- 2 --- 3 --- 4
    subgraph 1[Check Recovery Phrase]
        direction TB
        1.1["How long is your Recovery Phrase?"] --> 1.2["Enter word no. 1 of 24 of your Recovery Phrase"]
        1.2 --> 1.3{"Valid, Mismatched or Invalid Recovery Phrase"}
        1.3 --> 1.4[Tap to dismiss]
    end
    subgraph 2[Generate Backup Shares]
        direction TB
        2.1[How the backup works] --> 2.2[Why your Phrase?] --> 2.3["How long is your Recovery Phrase?"]
        2.3 --> 2.4["Enter word no. 1 of 24 of your Recovery Phrase"]
        2.4 --> 2.5{"Valid, Mismatched or Invalid Recovery Phrase"}
        2.5 --> |Invalid or mismatched| 2.6[Tap to dismiss]
        2.5 --> |Tap to continue| 2.7
        subgraph 2.7[Generate the Shares]
            direction TB
            2.7.1[How many Shares?] --> 2.7.2["Enter number of SSKR Shares to generate (1 - 16)"]
            2.7.2 --> 2.7.3[What is a threshold?] --> 2.7.4[Enter threshold value]
            2.7.4 --> 2.7.5["SSKR · Shares: 3 · Threshold: 2 · 138 words to write"]
            2.7.5 --> 2.7.6["Enough of these Shares rebuild your Recovery Phrase"]
            2.7.6 --> |Generate Backup Shares| 2.7.7[SSKR Share 1 of 3]
            2.7.7 --> 2.7.8["Close and erase all 3 Shares?"]
        end
    end
    subgraph 3[Recover from Backup]
        direction TB
        3.1[How recovery works] --> 3.2["Enter Share 1 Word 1 of your SSKR Backup"]
        3.2 --> 3.3{"Valid, Mismatched or Invalid SSKR Shares"}
        3.3 --> |Invalid| 3.4[Tap to dismiss]
        3.3 --> |Valid or mismatched| 3.5["A Recovery Phrase will be shown"]
        3.5 --> |Continue anyway| 3.6[Recovery Phrase]
    end
    subgraph 4[Derive with BIP85]
        direction TB
        4.1[How BIP85 works] --> 4.2{Which BIP85 secret?}
        4.2 --> |BIP39| 4.3[Length of BIP39 Phrase?]
        4.2 --> |"Password (Base64) or (Base85)"| 4.4[Enter password length]
        4.2 --> |PIN| 4.5["How many digits in the PIN?"]
        4.3 --> 4.6[What is an index?]
        4.4 --> 4.6
        4.5 --> 4.6
        4.6 --> 4.7["Enter index (0 - 9,999,999)"]
        4.7 --> 4.8["BIP39 · 24 words · Index 0 · m/83696968'/39'/0'/24'/0'"]
        4.8 --> 4.9["Anyone who sees this secret can use it."]
        4.9 --> |Derive this secret| 4.10["BIP39 Phrase (Index #0) · m/83696968'/39'/0'/24'/0'"]
    end
```

Three things the diagram does not say on its own:

* **Going back.** Every screen carries a back arrow, and going back far enough reaches the home page — which erases whatever was typed on the way there.
* **Why recovery goes on from a mismatch.** Checking and backing up stop on anything but *Valid*; recovery does not, and that is deliberate. It rebuilds the phrase whenever the shares recombine, matching this Ledger or not, because the device that needs a backup is precisely the one that no longer holds the phrase.
* **The path.** The BIP-85 derivation path is drawn twice: on the review, where the parameters are still being chosen, and again above the derived secret, so that what gets written down carries the path that reproduces it.

### Nano S, Nano S+ and Nano X

There is no home page with a menu behind it. The idle screen is a single flow walked left and right with the two buttons, and *Version* and *Quit* are steps of that flow, beside the three intentions rather than behind them. BIP-85 is absent: no Nano draws a single BIP-85 screen.

```mermaid
---
title: Seed Tool menu flow - Nano S, Nano S+ and Nano X
---
flowchart LR
    1 --- 2 --- 3 --- 4 --- 5
    subgraph 1[Check Phrase on this Ledger]
        direction TB
        1.1["Select the number of words on your Recovery Sheet"] --> 1.2["Enter word #1"]
        1.2 --> 1.3{"Your Phrase: not valid, doesn't match or correct"}
        1.3 --> |is not valid| 1.4["Check length, order and spelling"]
        1.4 --> 1.5[Re-enter Phrase]
        1.3 --> |is correct| 1.6[Quit]
        1.3 --> |doesn't match| 1.7[Return to menu]
        1.5 --> 1.7
    end
    subgraph 2[Generate Backup Shares]
        direction TB
        2.1["Your Phrase is split into Shares. Keep them apart."] --> 2.2["This Ledger cannot read its Phrase. Enter it to split it."]
        2.2 --> 2.3["Select the number of words on your Recovery Sheet"] --> 2.4["Enter word #1"]
        2.4 --> 2.5{"Your Phrase: not valid, doesn't match or correct"}
        2.5 --> |is not valid| 2.6["Check length, order and spelling"] --> 2.7[Re-enter Phrase]
        2.5 --> |doesn't match| 2.8["It would not restore this Ledger"] --> 2.9[Return to menu]
        2.5 --> |is correct| 2.10["Set up SSKR Backup"]
        2.10 --> 2.11
        subgraph 2.11[Generate the Shares]
            direction TB
            2.11.1["Select number of SSKR Shares"] --> 2.11.2["How many Shares rebuild your Phrase"]
            2.11.2 --> 2.11.3[Select threshold] --> 2.11.4["Review your Backup"]
            2.11.4 --> 2.11.5["SSKR: any 2 of 3 · 138 words to write"] --> 2.11.6["Shares will be shown"]
            2.11.6 --> 2.11.7["Make sure no one can see the screen"] --> 2.11.8["Enough Shares rebuild your Phrase"]
            2.11.8 --> |Generate Backup Shares| 2.11.9["SSKR 1/3"] --> 2.11.10["Written down all 3 Shares?"]
        end
    end
    subgraph 3[Recover from Backup]
        direction TB
        3.1["Not all your Shares are needed. Any order works."] --> 3.2["Enter first word of your first Share"]
        3.2 --> 3.3{"SSKR Shares: not valid, don't match or correct"}
        3.3 --> |are not valid| 3.4["Check length, order and spelling"] --> 3.5[Re-enter Shares]
        3.3 --> |"don't match or are correct"| 3.6["Phrase will be shown"]
        3.6 --> 3.7["Make sure no one can see the screen"] --> 3.8["Not necessarily this Ledger's Phrase"]
        3.8 --> |Show the Phrase| 3.9["Your Phrase"]
    end
    subgraph 4[Version]
        direction TB
        4.1[Version]
    end
    subgraph 5[Quit]
        direction TB
        5.1[Quit]
    end
```

A Nano S has two lines where the others have three, so several of those sentences are shortened on it.

> [!NOTE]
> This repository also holds [demo videos](demos/README.md) and [animations](tests/functional/screenshots/README.md) of the menu flows. Both were recorded in 2024, on Nano S and Stax only, from an interface that has since been redrawn — they show the menus the two diagrams above replaced, and are kept as a record of that version rather than as a picture of this one.

## Check BIP-39
The application invites the user to type a [BIP-39](https://github.com/bitcoin/bips/blob/master/bip-0039.mediawiki) mnemonic on their Ledger device. The BIP-39 mnemonic is compared to the onboarded seed and the application notifies the user whether both seeds match or not.

## Generate Shamir's secret sharing
If the user provided seed is valid and matches the onboarded seed, the user can create [Shamir's secret sharing (SSS)](https://en.wikipedia.org/wiki/Shamir%27s_secret_sharing) from their BIP-39 phrase.
The application uses [Sharded Secret Key Reconstruction (SSKR)](https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-011-sskr.md), an interoperable implementation of [Shamir's Secret Sharing (SSS)](https://en.wikipedia.org/wiki/Shamir%27s_secret_sharing). This provides a way for you to divide or 'shard' the master seed underlying a Bitcoin HD wallet into 'shares', which you can then distribute to friends, family, or fiduciaries. If you lose your seed, you can reconstruct it by collecting a sufficient number of your shares (the 'threshold'). Knowledge of fewer than the required number of parts ensures that information about the master secret is not leaked.

* SSKR is round-trip compatible with BIP-39.
* SSKR is based on SLIP-39, developed by SatoshiLabs. It is an improvement on, but is incompatible with, SLIP-39.
* SSKR phrases use a dictionary of exactly 256 English words with a uniform word size of 4 letters.
* SSKR encodes a [CBOR] structure tagged with the data type [URTYPES], and is therefore self-describing.
* Phrases generated by SSKR can be up to 46 words in length i.e. 184 characters.
* Only two letters of each word (the first and last) are required to uniquely identify each byte value, making a minimal [ByteWords](https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-012-bytewords.md) encoding as efficient as hexadecimal (2 characters per byte) and yet less error prone.
* Additionally, words can be uniquely identified by their first three letters or last three letters.
* Minimizing the number of letters for each word simplifies transfer to permanent media such as stamped metal.

For more information about SSKR, see [SSKR for Users](https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-users.md).

### Which shares this application can read

Shares generated by this application are ordinary SSKR shares in the encoding [BCR-2020-011](https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-011-sskr.md) describes, and other SSKR tools read them. What the application can read *back* is narrower than what the specification allows, in three ways. All three are restrictions on recovery only: generation is unaffected.

* **One group per backup.** `SSKR_MAX_GROUP_COUNT` is 1 here, where the specification gives `group-count` four bits and the upstream [bc-sskr](https://github.com/BlockchainCommons/bc-sskr) library allows 16. A backup whose shares are spread over several groups cannot be recombined by this application — including the worked example of BCR-2020-011 itself, and any multi-group configuration produced by Gordian SeedTool. The application never generates one: both interfaces produce a single group.
* **A threshold above 10 cannot be recovered on Nano S.** `SSS_MAX_SHARE_COUNT` is 10 on Nano S and 16 on every other supported device. Recovery asks for exactly as many shares as the threshold, so a set whose *threshold* is 11 or more can be recovered on a Nano S+, Nano X, Stax, Flex or Nano Gen5 but not on a Nano S. The number of shares is not what matters: a 3-of-12 set is recoverable everywhere, since only three shares are ever entered. Generation stays inside each device's own limit — the Nano S offers 1 to 7 shares, the other devices 1 to 16.
* **CBOR tag 40309 only.** Shares carrying the superseded tag 309 (`d9 01 35`) are refused. The specification marks that version deprecated and its support optional, so refusing it is conformant, but it does mean the shares published in Blockchain Commons' own [SSKR test vectors](https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md) are not readable as printed.

In each case the result is that a legitimate backup is refused, not that a secret is lost — and the message the application shows does not distinguish such a refusal from a mistyped word.

> [!NOTE]
> SSKR is non-deterministic. There is a random factor introduced when the shares are created, which means that every time you generate shares they will be different. This is an expected and correct result.

> [!TIP]
> Generated Shamir's Secret Shares may be cheaply and safely backed up to a steel wallet using the methods described [here](https://blockmit.com/english/guides/diy/make-cold-wallet-washers/) or [here](https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-cold-storage.md). This will keep your backup safe in event of fire, flood or natural disaster.

## Check Shamir's secret shares
The Ledger application also provides an option to confirm the onboarded seed against SSKR shares.

## Recover BIP-39
When the Shamir's secret shares have been validated the user can recover the BIP-39 phrase derived from those shares. This option takes advantage of SSKR's ability to perform a BIP-39 <-> SSKR round trip. If a user has lost or damaged their original Ledger device they may need to recover their BIP-39 phrase on another secure device. A BIP-39 phrase may still be recovered even if the SSKR phrases do not match the onboarded seed of a device but are still valid SSKR shares.

## Generate [BIP-85](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki)
BIP-85 is a standard built on top of [BIP-32](https://github.com/bitcoin/bips/blob/master/bip-0032.mediawiki) that enables the creation of child keys from a single parent seed. The protocol uses derivation paths to index the BIP-85 child keys. This ensures that you will always get the same child key if you follow the same derivation path from the same parent seed.

The BIP-85 standard describes several applications that use a fully hardened derivation path to create child keys supporting various formats such as BIP-39, xprv, hex, base64 passwords, base85 passwords, dice rolls and more.

> [!NOTE]
> A more detailed description of the BIP-85 standard may be found here:
> [BIP-85: Deterministic Entropy From BIP-32 Keychains](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki)

Due to the limitations of some of the older and smaller Ledger devices, not all BIP-85 applications are supported by all devices. The following table lists the various BIP-85 applications available on different Ledger devices:

<div align="center">

||Nano S|Nano S+|Nano X|Stax|Flex|Nano Gen5|
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
|[BIP-39](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki#user-content-BIP-39)|$${\color{red}✗}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|
|[PWD BASE64](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki#user-content-PWD_BASE64)|$${\color{red}✗}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|
|[PWD BASE85](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki#user-content-PWD_BASE85)|$${\color{red}✗}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|$${\color{green}✓}$$|
|[HEX](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki#user-content-HEX)|$${\color{red}✗}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|
|[DICE](https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki#user-content-DICE)|$${\color{red}✗}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|$${\color{orange}✓}$$|
</div>

> [!TIP]
> Safely, securely and redundantly backup a parent root seed using Shamir's Secret Sharing. Use BIP-85's BIP-39 application to create several child wallets from that parent root seed. Further protect those child wallets using passwords generated using the BIP-85 PWD BASE64 or PWD BASE85 applications. On Ledger devices those wallets can also then be protected by a PIN generated using the BIP-85 DICE application.

```mermaid
---
title: One Seed to rule them all - Multi wallet
---
flowchart TB
    1.1 --> |Backup| 1.2
    1 --> |BIP-85 Child 0| 2.1.1
    1 --> |BIP-85 Child 1| 2.1.2
    1 --> |BIP-85 Child 2| 2.2.1
    1 --> |BIP-85 Child 3| 2.2.2
    1 --> |BIP-85 Child 4| 2.3.1
    1 --> |BIP-85 Child 5| 2.3.2
    1 --> |BIP-85 Child 6| 2.4.1
    1 --> |BIP-85 Child 7| 2.4.2
    subgraph 1[Parent]
        direction TB
        1.1[Root Seed]
        subgraph 1.2[2-of-3 Shamir's Secret Shares]
            direction BT
            1.2.1[Share 1]
            1.2.2[Share 2]
            1.2.3[Share 3]
        end
    end
    subgraph 2[Children]
        direction TB
        subgraph 2.1[Cold Wallet]
            direction LR
            2.1.1[BIP-39 #1]
            2.1.2[Password #1]
            end
            subgraph 2.2[Hardware Wallet]
            direction LR
            2.2.1[BIP-39 #2]
            2.2.2[Password #2]
            end
            subgraph 2.3[Lightning Wallet]
            direction LR
            2.3.1[BIP-39 #3]
            2.3.2[Password #3]
            end
            subgraph 2.4[Phone Wallet]
            direction LR
            2.4.1[BIP-39 #4]
            2.4.2[Password #4]
            end
    end
```
```mermaid
---
title: One Seed to rule them all - MultiSig
---
flowchart TB
    1.1 --> |Backup| 1.2
    1 --> |BIP-85 Child 0| 2.1.1
    1 --> |BIP-85 Child 1| 2.1.2
    1 --> |BIP-85 Child 2| 2.2.1
    1 --> |BIP-85 Child 3| 2.2.2
    1 --> |BIP-85 Child 4| 2.3.1
    1 --> |BIP-85 Child 5| 2.3.2
    2.1 --> 3.1
    2.2 --> 3.2
    2.3 --> 3.3
    subgraph 1[Parent]
        direction TB
        1.1[Root Seed]
        subgraph 1.2[2-of-3 Shamir's Secret Shares]
            direction BT
            1.2.1[Share 1]
            1.2.2[Share 2]
            1.2.3[Share 3]
        end
    end
    subgraph 2[Children]
        direction TB
        subgraph 2.1[Wallet #1]
            direction LR
            2.1.1[BIP-39 #1]
            2.1.2[Password #1]
            end
            subgraph 2.2[Wallet #2]
            direction LR
            2.2.1[BIP-39 #2]
            2.2.2[Password #2]
            end
            subgraph 2.3[Wallet #3]
            direction LR
            2.3.1[BIP-39 #3]
            2.3.2[Password #3]
            end
    end
    subgraph 3[2-of-3 MultiSig Wallet]
        direction LR
        3.1[Signer 1]
        3.2[Signer 2]
        3.3[Signer 3]
    end
```
