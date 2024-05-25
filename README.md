<h1 align="center">Flipper Zero: Application Template</h1>

## How to use this template
1. Setup the repository by clicking the `Use this template` button on the top of the repository. Fill in the data if needed.
2. Update `README.md`'s upstream url with your repository's url.
3. Add `LICENSE` file with your own license.
4. Update `application.fam` with your application's information.

## Build Status
<!-- Replace the https://github.com/Alex4386/f0-template to your own repo after using template! -->
* **Latest Release**: [Download](https://github.com/Alex4386/f0-template/releases/latest)
* **Latest Nightly**: [Download](https://github.com/Alex4386/f0-template/actions/workflows/nightly.yml) _(GitHub Login Required)_

| Nightly Build | Release Build |
|:-------------:|:-------------:|
| ![Nightly Build](https://github.com/Alex4386/f0-template/actions/workflows/nightly.yml/badge.svg) | ![Release Build](https://github.com/Alex4386/f0-template/actions/workflows/release.yml/badge.svg) |

## Setup Build environment
### Build Instruction
1. Install `ufbt`:
    ```bash
    pip3 install ufbt
    ```
2. Clone this repository and enter the repository root.
3. Run `ufbt update` to update the SDK for your flipper
    - If you are using custom firmware, You should switch SDK. Here is the example for `unleashed` firmware:
        ```bash
        ufbt update --index-url=https://up.unleashedflip.com/directory.json
        ```
    - If you want to use different release channel, You can run update to that channel too. Here is the example for `dev` channel (`dev`, `rc`, `release` are supported):
        ```bash
        ufbt update --channel=dev
        ```
4. Run `ufbt` in the repository root:
    ```bash
    ufbt
    ```
5. Compiled binary is now available at `./dist/` directory.

### Setup Visual Studio Code
> [!WARNING]
> This command will overwrite your `.vscode` directory and `.gitignore` on your root directory. 
> **Make sure to backup your changes before running this command.**

1. Suppose your build environment is ready.
2. Run `ufbt vscode_dist` to generate Visual Studio Code config.

## Developer Resources
Here are the resources for developing applications for Flipper Zero:
- [Flipper Zero Firmware Docs](https://developer.flipper.net/flipperzero/doxygen/)
  - [`struct` list](https://developer.flipper.net/flipperzero/doxygen/annotated.html) ([index](https://developer.flipper.net/flipperzero/doxygen/classes.html))
- [Flipper Zero code examples](https://github.com/m1ch3al/flipper-zero-dev-tutorial)
