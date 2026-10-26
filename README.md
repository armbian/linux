# armbian/linux — kernel.org linux-stable mirror

Exact GitHub mirror of the upstream **linux-stable** tree
(`git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git`) — every branch
(`master`, `linux-*.y`) and every tag.

Kept in sync by [`.github/workflows/mirror-sync.yml`](.github/workflows/mirror-sync.yml)
(every 30 min). Armbian builds point `MAINLINE_MIRROR` here so kernels are
fetched from a mirror we control, instead of a third-party one whose branches lag.

The sync workflow lives on the **`armbian-mirror`** branch; all other branches
are upstream mirrors — do not commit to them.
