{
  description = "tmp-hons";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = {
    self,
    nixpkgs,
  }: let
    systems = ["x86_64-linux"];
    forAllSystems = nixpkgs.lib.genAttrs systems;
  in {
    # packages = forAllSystems (
    #   system: let
    #     pkgs = nixpkgs.legacyPackages.${system};
    #     stdenv = pkgs.stdenv;
    #   in {
    #     default = stdenv.mkDerivation {
    #       name = "tmp-hons";
    #       src = ./src;
    #       buildInputs = [pkgs.libx11];
    #       nativeBuildInputs = [pkgs.cmake];
    #     };
    #   }
    # );
    #
    devShells = forAllSystems (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        stdenv = pkgs.clangStdenv;
        python = pkgs.python3.override {
          self = python;
          packageOverrides = pyfinal: pyprev: {
            pddl = pyfinal.callPackage ./pddl.nix {};
          };
        };
      in {
        default = pkgs.mkShell {
          packages = [
            (python.withPackages (p: [
              p.pybullet
              p.pddl
              p.z3-solver
              p.matplotlib
            ]))
            pkgs.fast-downward
            pkgs.pyright
            pkgs.black
            pkgs.cmake
            pkgs.clang
            pkgs.bisoncpp
            pkgs.bison
            pkgs.z3
          ];
        };
      }
    );
  };
}
