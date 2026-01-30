{
  description = "A modern, cross-platform desktop application for viewing and editing TXD (Texture Dictionary) files from GTA games";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        
        # Application version
        version = "1.0.2";
        
      in
      {
        packages = {
          default = self.packages.${system}.txdedit;
          
          txdedit = pkgs.stdenv.mkDerivation {
            pname = "txdedit";
            inherit version;
            
            src = ./.;
            
            nativeBuildInputs = with pkgs; [
              cmake
              pkg-config
              qt6.wrapQtAppsHook
              git
            ];
            
            buildInputs = with pkgs; [
              qt6.qtbase
              qt6.qttools
              gtest
            ];
            
            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
            ];
            
            # Patch CMakeLists.txt to use system gtest instead of FetchContent
            postPatch = ''
              substituteInPlace CMakeLists.txt \
                --replace-fail "include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL \"\" FORCE)
FetchContent_MakeAvailable(googletest)" "find_package(GTest REQUIRED)"
            '';
            
            # Install phase
            installPhase = ''
              mkdir -p $out/bin
              cp bin/txdedit $out/bin/
              
              # Copy icons and logos
              mkdir -p $out/share/txdedit
              cp -r ${./icons} $out/share/txdedit/icons
              cp -r ${./logos} $out/share/txdedit/logos
              
              # Create desktop entry
              mkdir -p $out/share/applications
              cat > $out/share/applications/txdedit.desktop << EOF
              [Desktop Entry]
              Type=Application
              Name=TXD Edit
              Comment=TXD (Texture Dictionary) file editor for GTA games
              Exec=$out/bin/txdedit
              Terminal=false
              Categories=Graphics;Viewer;Editor;
              Icon=$out/share/txdedit/icons/linux.png
              EOF
            '';
            
            meta = with pkgs.lib; {
              description = "Modern, cross-platform desktop application for viewing and editing TXD files from GTA games";
              homepage = "https://github.com/vaibhavpandeyvpz/txdedit";
              license = licenses.mit;
              platforms = platforms.linux ++ platforms.darwin;
              mainProgram = "txdedit";
            };
          };
        };
        
        # Development shell
        devShells.default = pkgs.mkShell {
          name = "txdedit-dev";
          
          buildInputs = with pkgs; [
            cmake
            pkg-config
            qt6.qtbase
            qt6.qttools
            qt6.qtcreator
            gtest
            clang-tools
          ];
          
          shellHook = ''
            echo "TXD Edit development environment"
            echo "================================"
            echo "Qt version: ${pkgs.qt6.qtbase.version}"
            echo "CMake version: ${pkgs.cmake.version}"
            echo ""
            echo "To build:"
            echo "  mkdir build && cd build"
            echo "  cmake .."
            echo "  make"
            echo ""
            echo "To run tests:"
            echo "  cd build"
            echo "  ./txd_tests"
            echo ""
          '';
        };
        
        # Apps for easy running
        apps = {
          default = self.apps.${system}.txdedit;
          
          txdedit = {
            type = "app";
            program = "${self.packages.${system}.txdedit}/bin/txdedit";
          };
        };
      }
    );
}
