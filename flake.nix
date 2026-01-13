{
  description = "Fan control utilities for Dell laptops";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages = {
          default = pkgs.stdenv.mkDerivation rec {
            pname = "i8kutils";
            version = "1.43";

            src = ./.;

            buildInputs = with pkgs; [ tcl acpi ];

            makeFlags = [ ];

            buildPhase = ''
              runHook preBuild
              make
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall

              # Create directories
              mkdir -p $out/bin
              mkdir -p $out/share/man/man1
              mkdir -p $out/etc/modprobe.d

              # Install binaries
              install -m 755 i8kctl $out/bin/
              install -m 755 i8kmon $out/bin/
              install -m 755 i8kfan $out/bin/

              # Install man pages
              install -m 644 i8kctl.1 $out/share/man/man1/
              install -m 644 i8kmon.1 $out/share/man/man1/

              # Install configuration files
              install -m 644 i8kmon.conf $out/etc/
              install -m 644 dell-smm-hwmon.conf $out/etc/modprobe.d/

              runHook postInstall
            '';

            meta = with pkgs.lib; {
              description = "Fan control utilities for Dell laptops";
              longDescription = ''
                i8kutils is a collection of utilities to control Dell laptops fans.
                It includes programs to turn the fans on and off, to read fans status,
                CPU temperature, and BIOS version.

                Note: Requires the dell-smm-hwmon kernel module to be loaded.
              '';
              homepage = "https://launchpad.net/i8kutils";
              license = licenses.gpl2Plus;
              platforms = platforms.linux;
              maintainers = [ ];
            };
          };
        };

        # Development shell
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            gcc
            gnumake
            tcl
            acpi
          ];

          shellHook = ''
            echo "i8kutils development environment"
            echo "Run 'make' to build"
            echo ""
            echo "Note: The dell-smm-hwmon kernel module must be loaded:"
            echo "  sudo modprobe dell-smm-hwmon"
          '';
        };
      }
    ) // {
      # NixOS module for easy integration
      nixosModules.default = { config, lib, pkgs, ... }:
        with lib;
        let
          cfg = config.services.i8kmon;
        in {
          options.services.i8kmon = {
            enable = mkEnableOption "i8kmon fan control daemon for Dell laptops";

            package = mkOption {
              type = types.package;
              default = self.packages.${pkgs.system}.default;
              description = "The i8kutils package to use";
            };
          };

          config = mkIf cfg.enable {
            boot.kernelModules = [ "dell-smm-hwmon" ];
            environment.systemPackages = [ cfg.package ];

            systemd.services.i8kmon = {
              description = "Dell laptop thermal monitoring";
              wantedBy = [ "multi-user.target" ];
              serviceConfig = {
                Type = "simple";
                ExecStart = "${cfg.package}/bin/i8kmon";
                Restart = "on-failure";
              };
            };
          };
        };
    };
}
