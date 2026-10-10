{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  inputs.espidf.url = "github:hnaderi/espidf-nix";

  outputs =
    { nixpkgs, espidf, ... }:
    let
      pkgs = import nixpkgs {
        system = "x86_64-linux";
        overlays = [ espidf.overlays.default ];
      };
    in
    {
      devShells.x86_64-linux.default = pkgs.mkShell {
        packages = [
          (espidf.lib.mkEspIdf {
            inherit pkgs;
            idfVersion = "v5.5.4";
            extraTools = [ "xtensa-esp32-elf-gdb" ];
            extraPkgs = pkgs: [ pkgs.qemu ];
          })
        ];
      };
    };
}
