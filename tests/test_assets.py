"""Prove the generated headers match their upstream sources exactly."""
import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
GAMMA_H = ROOT / "components" / "galactic_unicorn" / "gu_gamma.h"
PIO_H = ROOT / "components" / "galactic_unicorn" / "gu_pio.h"

# Assembled from tools/galactic_unicorn.pio by adafruit_pioasm.
EXPECTED_PIO = [
    0x6048, 0x6008, 0x7121, 0xE004, 0x0026, 0xE005, 0xBA42, 0x7121,
    0xE004, 0x002B, 0xE005, 0xBA42, 0x7121, 0xE004, 0x0030, 0xE005,
    0x7A65, 0x0082, 0x6068, 0xE506, 0xE000, 0x6040, 0x0096, 0xE004,
]


def _ints(path, symbol):
    src = path.read_text()
    body = re.search(symbol + r"\[[^\]]*\]\s*=\s*\{(.*?)\}", src, re.S).group(1)
    return [int(tok, 0) for tok in re.findall(r"0x[0-9A-Fa-f]+|\d+", body)]


@pytest.fixture(autouse=True, scope="module")
def regenerate():
    subprocess.run([sys.executable, str(ROOT / "tools" / "generate_assets.py")], check=True)


def test_gamma_matches_pimoroni_formula():
    table = _ints(GAMMA_H, "GALACTIC_UNICORN_GAMMA_14BIT")
    assert len(table) == 256
    expected = [round(pow(i / 255, 2.2) * 16383) for i in range(256)]
    assert table == expected


def test_gamma_endpoints():
    table = _ints(GAMMA_H, "GALACTIC_UNICORN_GAMMA_14BIT")
    assert table[0] == 0
    assert table[255] == 16383
    # Spot values copied from pimoroni-pico common/pimoroni_common.hpp.
    assert table[:16] == [0, 0, 0, 1, 2, 3, 4, 6, 8, 10, 13, 16, 20, 23, 28, 32]


def test_pio_program_matches_upstream():
    assert _ints(PIO_H, "galactic_unicorn_program_instructions") == EXPECTED_PIO


def test_pio_config_constants_present():
    src = PIO_H.read_text()
    # .side_set 1 opt assembles to a 2-bit sideset field with optional=true.
    assert "sm_config_set_sideset(&c, 2, true, false)" in src
    assert "sm_config_set_wrap(&c, offset + 0, offset + 23)" in src
