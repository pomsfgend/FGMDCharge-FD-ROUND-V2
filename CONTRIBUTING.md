# Contributing

Pull requests go to `develop`. `main` is the published tree.

Write in English, Russian, Chinese, Spanish, Portuguese, German, French, Japanese, or Korean. The issue template accepts the same set.

There is no single author. The tree was vibe-coded from the factory dump and from time spent on real stations. The dump in `original/` is not MIT and was not written here.

Do not add a MAC address, a home directory, or a hard-coded COM port.

Before a firmware change:

- `python tools/static_audit.py`
- `python host_tests/test_core.py`
- If the release number changes, it must match `main/config.h`.

Do not tag an old version number on today's source. Historical packages that still exist live in `archive/`.
