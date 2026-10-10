## What changed

Target branch: `develop`.

Do not include a MAC address, a home directory, or a hard-coded COM port.

## Checks

- [ ] `python tools/static_audit.py`
- [ ] `python host_tests/test_core.py`
- [ ] Version string matches `main/config.h` if the release number changed
