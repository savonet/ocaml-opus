(lang dune 2.8)
(version 0.3.0)
(name opus)
(source (github savonet/ocaml-opus))
(license GPL-2.0)
(authors "The Savonet Team <savonet-users@lists.sourceforge.net>")
(maintainers "The Savonet Team <savonet-users@lists.sourceforge.net>")

(generate_opam_files true)
(use_standard_c_and_cxx_flags false)

(package
 (name opus)
 (synopsis "Bindings to libopus")
 (depends
  conf-libogg
  conf-libopus
  conf-pkg-config
  (ocaml (>= 4.08.0))
  dune
  dune-configurator
  (ogg (>= 0.7.1)))
)
