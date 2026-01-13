{ stdenv
, lib
, tcl
, acpi
}:

stdenv.mkDerivation rec {
  pname = "i8kutils";
  version = "1.43";

  src = ./.;

  buildInputs = [ tcl acpi ];

  # Use the existing Makefile
  makeFlags = [ ];

  # The Makefile only builds i8kctl by default
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

  meta = with lib; {
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
}
