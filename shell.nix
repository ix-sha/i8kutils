{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
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
    echo "Note: The dell-smm-hwmon kernel module must be loaded for the utilities to work:"
    echo "  sudo modprobe dell-smm-hwmon"
  '';
}
