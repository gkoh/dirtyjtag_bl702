# Nix development shell for the DirtyJTAG BL702 port.
#
# Provides the Xuantie/T-Head RISC-V bare-metal cross-toolchain (the patched
# GCC 10.2.0 that bouffalo_sdk requires, with CLIC/mtvt CSR support) plus
# cmake/make/git.
#
# The toolchain is fetched from the bouffalolab/toolchain_gcc_t-head_linux
# repository, which ships prebuilt riscv64-unknown-elf-* binaries. This is
# the toolchain the SDK expects — standard nixpkgs riscv64-none-elf-gcc lacks
# the mtvt CSR and xthead extensions the SDK's startup assembly uses.
#
# Usage:
#   nix-shell default.nix
#   make                      # CHIP=bl702 BOARD=tang_primer_20k defaults
#
# This shell does NOT build the firmware; it only provides the environment.
{ pkgs ? import <nixpkgs> {} }:

let
  # The bouffalo_sdk requires the Xuantie/T-Head patched GCC toolchain
  # (riscv64-unknown-elf-gcc 10.2.0). Standard nixpkgs riscv64-none-elf-gcc
  # lacks the mtvt CSR and custom extensions used by the SDK's startup code.
  # We fetch the prebuilt binaries directly from Bouffalo Lab's toolchain repo.
  xuantieToolchain = pkgs.stdenv.mkDerivation {
    name = "xuantie-riscv64-toolchain";
    src = pkgs.fetchFromGitHub {
      owner = "bouffalolab";
      repo = "toolchain_gcc_t-head_linux";
      rev = "c4afe91cbd01bf7dce525e0d23b4219c8691e8f0";
      sha256 = "sha256-xlRFjZ0LKAsa+QFsPAWQeJIXzwb2e1E5KDDYRfjJDTU=";
    };

    nativeBuildInputs = [ pkgs.autoPatchelfHook ];

    installPhase = ''
      runHook preInstall
      mkdir -p $out
      cp -r * $out/
      runHook postInstall
    '';

    # Libraries the prebuilt binaries depend on
    buildInputs = [
      pkgs.stdenv.cc.cc.lib
      pkgs.zlib
      pkgs.ncurses5  # for riscv64-unknown-elf-gdb (libncursesw.so.5)
    ];
  };

in
pkgs.mkShell {
  nativeBuildInputs = [
    xuantieToolchain
    pkgs.cmake
    pkgs.gnumake
    pkgs.git
    pkgs.clang-tools  # provides clang-format for the style check
  ];

  # The SDK Makefile reads BL_SDK_BASE relative to the project root; the
  # submodule is at ./bouffalo_sdk. No env override needed for the dev shell.
  hardeningDisable = [ "all" ];

  # Nix sets CONFIG_SHELL (for autoconf) to the Nix bash path; the SDK's
  # Makefile treats any CONFIG_* variable as a Kconfig option and writes it
  # to autoconf.h, breaking the IS_ENABLED() macro. Unset it.
  shellHook = ''
    unset CONFIG_SHELL
  '';
}
