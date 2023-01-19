PVXS - PVAccess protocol library
================================
[![PVXS EPICS](https://github.com/george-mcintyre/pvxs/actions/workflows/ci-scripts-build.yml/badge.svg)](https://github.com/george-mcintyre/pvxs/actions/workflows/ci-scripts-build.yml)
[![PVXS Python](https://github.com/george-mcintyre/pvxs/actions/workflows/python.yml/badge.svg)](https://github.com/george-mcintyre/pvxs/actions/workflows/python.yml)

# PVXS client/server for PVA Protocol
This module provides a library (libpvxs.so or pvxs.dll) and a set of CLI utilities acting as PVAccess protocol client and/or server.

PVXS is functionally equivilant to the pvDataCPP and pvAccessCPP modules.

## References
- Source: https://github.com/mdavidsaver/pvxs
- Documentation: https://mdavidsaver.github.io/pvxs

## Dependencies
- A C++11 compliant compiler 
- GCC >= 4.8
- Visual Studio >= 2015 / 12.0 
- EPICS Base >=3.15.1 
- libevent >=2.0.1 (Optionally bundled)
- (optional) CMake >=3.1, only needed when building bundled libevent
