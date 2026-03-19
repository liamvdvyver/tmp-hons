{
  lib,
  pkgs,
  buildPythonPackage,
  fetchPypi,
  setuptools,
  wheel,
}:
with pkgs.python3Packages;
  buildPythonPackage rec {
    pname = "pddl";
    version = "0.4.6";

    src = fetchPypi {
      inherit pname version;
      hash = "sha256-YVA+AIYUUz9DuCnpnm+cKgEA9PCtz54L0pc2iIMMCRI=";
    };

    # do not run tests
    doCheck = false;

    # specific to buildPythonPackage, see its reference
    pyproject = true;
    build-system = [
      setuptools
      wheel
    ];
    propagatedBuildInputs = [
      click
      (buildPythonPackage rec {
        pname = "lark";
        version = "1.1.9"; # from Pipfile.lock
        src = fetchPypi {
          inherit pname version;
          hash = "sha256-FfpSNkkIJMLEq6DiLS1tgjV13K9M3RhI40tq2DYkD7o=";
        };
        doCheck = false;
        pyproject = true;
        build-system = [setuptools wheel];
      })
    ];
  }
