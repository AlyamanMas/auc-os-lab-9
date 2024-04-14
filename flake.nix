{
  inputs = { utils.url = "github:numtide/flake-utils"; };
  outputs = { self, nixpkgs, utils }:
    utils.lib.eachDefaultSystem (system:
      let pkgs = nixpkgs.legacyPackages.${system};
      in {
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [ gcc ];
          nativeBuildInputs = with pkgs; [
            clang-tools
            man-pages
            man-pages-posix
            gnumake
            gcc
          ];
        };
      });
}
