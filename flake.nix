{
  description = "ftr — minimal Fuchsia FXT trace writer for C/C++";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        packages = rec {
          ftr = pkgs.stdenv.mkDerivation {
            pname = "ftr";
            version = "0.1.0";
            src = ./.;
            nativeBuildInputs = [ pkgs.cmake ];
            cmakeFlags = [ "-DFTR_BUILD_EXAMPLES=OFF" ];
          };
          default = ftr;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.ftr ];
          packages = [ pkgs.cmake ];
        };
      });
}
