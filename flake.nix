{
  description = "cavecarto flake.nix";

  inputs.nixpkgs.url = "nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in {
      packages.${system}.default = pkgs.clangStdenv.mkDerivation {
        pname = "cavecarto";
        version = "0.0.1";
        src = self;

        buildInputs = [
          pkgs.pkg-config
          pkgs.sqlite
        ];

        configurePhase = "echo no configure";
        buildPhase = "make";

        installPhase = ''
          mkdir -p $out/bin
          cp bin/* $out/bin/
        '';
      };
    };
}
