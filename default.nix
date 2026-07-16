{ pkgs ? import <nixpkgs> {} }:

let
  lib = pkgs.lib;

  libxfce4ui_421 = pkgs.libxfce4ui.overrideAttrs (_: {
    version = "4.21.8";
    src = pkgs.fetchurl {
      url = "https://archive.xfce.org/src/xfce/libxfce4ui/4.21/libxfce4ui-4.21.8.tar.xz";
      sha256 = "sha256-5sqvxv5eB3ZeIt2EmhmyHlqkT1HGb6HX23S5QzC9RXs=";
    };

    nativeBuildInputs = with pkgs; [
      gettext
      meson
      ninja
      pkg-config
      python3
      wrapGAppsHook3
    ];

    configureFlags = [];

    mesonFlags = [
      "-Dgtk-doc=false"
      "-Dintrospection=false"
      "-Dvala=disabled"
      "-Dvendor-info=NixOS"
    ];

    # The 4.21.8 Meson build incorrectly records public header dependencies
    # as private, so consumers do not receive the libxfce4util include path.
    postPatch = ''
      patchShebangs xdt-gen-visibility
      substituteInPlace libxfce4ui/meson.build \
        --replace-fail \
          "description: 'Widgets library for Xfce'," \
          "description: 'Widgets library for Xfce', requires: [gtk, libxfce4util],"
    '';
  });

  cleanSource = lib.cleanSourceWith {
    src = ./.;
    filter = path: type:
      let
        rel = lib.removePrefix ((toString ./. ) + "/") (toString path);
      in
        lib.cleanSourceFilter path type
        && !(lib.hasPrefix ".cache/" rel)
        && !(lib.hasPrefix "build/" rel)
        && !(lib.hasPrefix "build-clean/" rel)
        && !(lib.hasPrefix "autom4te.cache/" rel);
  };
in
pkgs.stdenv.mkDerivation (finalAttrs: {
  pname = "thunar";
  version = "4.21.5-dev-local";

  outputs = [
    "out"
    "dev"
  ];

  src = cleanSource;

  nativeBuildInputs = with pkgs; [
    meson
    ninja
    gettext
    docbook_xsl
    libxslt
    pkg-config
    xfce4-dev-tools
    wrapGAppsHook3
    gobject-introspection
  ];

  buildInputs = with pkgs; [
    xfce4-exo
    glib
    gdk-pixbuf
    gtk3
    libx11
    libsm
    libice
    libexif
    libgudev
    libcanberra
    libnotify
    libxfce4ui_421
    libxfce4util
    polkit
    gexiv2
    pcre2
    vte
    xfce4-panel
    xfconf
    pango
  ];

  mesonFlags = [
    "-Dgtk-doc=false"
    "-Dtests=false"
  ];

  enableParallelBuilding = true;

  preFixup = ''
    gappsWrapperArgs+=(
      --prefix PATH : ${lib.makeBinPath [ pkgs.xfce4-exo ]}
    )
  '';

  meta = with lib; {
    description = "Local wrapped build of the Xfce file manager";
    homepage = "https://gitlab.xfce.org/xfce/thunar";
    license = licenses.gpl2Plus;
    mainProgram = "thunar";
    platforms = platforms.linux;
  };
})
