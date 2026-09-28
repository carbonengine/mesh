
This project includes third-party code as git submodules under `vendor/github.com/`. They are used by the
optional tools (`BUILD_TOOLS=ON`) only, and each keeps its own license file:

- [ufbx](https://github.com/ufbx/ufbx) (`vendor/github.com/ufbx/ufbx`), compiled into `cmfprocessor`.
    > Copyright (c) 2020 Samuli Raivio. Available under the MIT License or Public Domain (Unlicense), at your choice.

- [Font Awesome Free](https://github.com/FortAwesome/Font-Awesome) (`vendor/github.com/FortAwesome/Font-Awesome`), icon font used by the viewer.
    > Copyright (c) 2024 Fonticons, Inc. Fonts under the SIL Open Font License 1.1, icons under CC BY 4.0, code under the MIT License.

- [MoltenVK](https://github.com/KhronosGroup/MoltenVK) (`vendor/github.com/moltenVK`), Vulkan driver for the viewer on macOS.
    > Copyright (c) 2015-2025 The Brenwill Workshop Ltd. Licensed under the Apache License 2.0.
