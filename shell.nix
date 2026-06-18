{ pkgs ? import <nixpkgs> {} }:

let
  loader = pkgs.lib.fileContents "${pkgs.stdenv.cc}/nix-support/dynamic-linker";
  libPath = pkgs.lib.makeLibraryPath [
    pkgs.stdenv.cc.cc.lib   # libc / libstdc++ / libgcc
    pkgs.zlib
    pkgs.openssl
  ];

  # `run ./tester ...` launches a prebuilt, dynamically linked binary by invoking
  # the glibc loader explicitly. A real executable, so it works in fish/bash/etc,
  # on any host, even without programs.nix-ld.
  run = pkgs.writeShellScriptBin "run" ''
    exec "${loader}" --library-path "${libPath}" "$@"
  '';
in
pkgs.mkShell {
  packages = [
    pkgs.python3
    pkgs.siege
    run
  ];
}
