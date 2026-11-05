{
  description = "Ruumble Client: Mumble 1.6 with the Ruumble office building as its window";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  # the Ruumble core and the icon come from the server/ submodule
  inputs.self.submodules = true;

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: rec {
        ruumble-client = pkgs.callPackage ./nix/package.nix { };
        default = ruumble-client;
      });

      # nixpkgs.overlays = [ ruumble-client.overlays.default ]; then pkgs.ruumble-client
      overlays.default = final: _prev: { ruumble-client = final.callPackage ./nix/package.nix { }; };

      # imports = [ ruumble-client.nixosModules.default ]; programs.ruumble-client.enable = true;
      nixosModules.default =
        { config, lib, pkgs, ... }:
        {
          options.programs.ruumble-client.enable = lib.mkEnableOption "Ruumble Client, the Mumble client with Ruumble built in";
          config = lib.mkIf config.programs.ruumble-client.enable {
            environment.systemPackages = [ self.packages.${pkgs.stdenv.hostPlatform.system}.default ];
          };
        };
    };
}
