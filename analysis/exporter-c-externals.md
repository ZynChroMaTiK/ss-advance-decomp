# Exporting C-only external references

The first export containing exact `109F0` failed to link `sub_0800036C` and
`D_030000F0`. The matcher resolves these canonical address names, but the
exported build previously gathered fallback addresses only from the imported
catalogue, named symbols and Luvdis assembly. A recovered C function can refer
to a valid thunk/data address that none of those sources names.

`patches/decomp-wtf-c-externals.patch` extends the existing Thumb-symbol patch.
The exporter now reads undefined symbols and branch relocations from compiled
C objects. Canonical `sub_`, `func_`, `D_` and underscore address names gain
fallback addresses, while existing explicitly recorded addresses take priority.
Unrecognized names still fail to link. `R_ARM_THM_CALL` and `R_ARM_THM_JUMP24`
identify Thumb targets; the existing `.thumb_set` emitter makes real function
symbols. Ordinary ARM calls retain their mode. Data symbols remain absolute
addresses. No instructions or exported ROM bytes are substituted.

`scripts/bootstrap.sh` applies both patches in order to the pinned checkout,
accepts a checkout already carrying both, and upgrades the previous one-patch
checkout. The Luvdis analyzer's unrelated stack-bound fixes remain unchanged.

`tests/test_export_c_externals.py` applies both patches to the pristine pinned
exporter and compares the resulting source. A real ARM object then exercises
C-only Thumb, ARM and data references. The generated thunk is `STT_FUNC` at
`0x0800036D`, the data address is `0x030000F0`, and linking adds no interworking
veneer bytes. Full-ROM export and a subsequent independent rebuild both pass
with **67/526** C functions and no assembly fallback for matched functions.
The 8 MiB output SHA-1 remains `4ff48d8c18b70a5184758f1917343ae85a70858e`.
Proof is recorded in `analysis/rebuild-validation.json` and
`decomp/export-result.json`.
