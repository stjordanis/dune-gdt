import pytest
from dune.xt.common.test import load_all_submodule


def test_load_all():
    import dune.gdt as gdt
    load_all_submodule(gdt)
