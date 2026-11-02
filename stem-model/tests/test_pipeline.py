import json
import os
import shutil
import subprocess
import sys

import numpy as np
import pytest
import torch

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import relations  # noqa: E402
from distractors import TEMPLATES, add_distractor  # noqa: E402
from export_int8 import GROUP, SCALE_BITS, dequantize, quantize  # noqa: E402
from gen_traces import render_call, run_call  # noqa: E402
from relations import RELATIONS  # noqa: E402
from tokenizer import PAD, SPECIALS, Tokenizer, train as train_tokenizer  # noqa: E402

GOLDEN = {
    ("quad_solve", (1, -3, 2)): "x=1, x=2",
    ("quad_solve", (2, -6, 4)): "x=1, x=2",
    ("deriv_poly3", (1, 0, 0, 0, 3)): "27",
    ("deriv_poly3", (2, -1, 0, 5, -1)): "8",
    ("integr_poly3", (3, -2, -3, -3, 2, 6)): "2284/3",
    ("integr_poly3", (1, 0, 0, 0, 0, 2)): "4",
    ("linear2", (1, 1, 7, 1, -1, 1)): "x=4, y=3",
    ("exp_solve", (2, 8)): "3",
    ("exp_solve", (10, 1000)): "3",
    ("kinematic_v", (5, 2, 4)): "13 m/s",
    ("ohm_i", (12, 4)): "3 A",
    ("ohm_i", (2, 3)): "0.6667 A",
    ("moles", (5, 100)): "0.05 mol",
    ("dilution", (1, 50, 250)): "0.2 M",
    ("dilution", (4, 10, 60)): "0.6667 M",
}


@pytest.mark.parametrize("key,expected", sorted(GOLDEN.items()))
def test_golden_answers(key, expected):
    name, params = key
    assert RELATIONS[name].solve(*params) == expected


def test_dec_has_no_binary_noise():
    assert relations.dec(0.2) == "0.2"
    assert relations.dec(2 / 3) == "0.6667"
    assert relations.dec(7.33616) == "7.336"
    assert relations.dec(0.05) == "0.05"


def test_fmt_keeps_rationals_exact():
    import sympy as sp

    assert relations.fmt(sp.Rational(2284, 3)) == "2284/3"
    assert relations.fmt(sp.Integer(27)) == "27"
    assert relations.fmt(sp.Integer(-4)) == "-4"


def test_poly_renders_naturally():
    assert relations.poly([1, -3, 2]) == "x^2 - 3x + 2"
    assert relations.poly([-2, 0, -3]) == "-2x^2 - 3"
    assert relations.poly([1, 0, 0]) == "x^2"
    assert relations.poly([0, 0, 0]) == "0"


def test_all_relations_roundtrip_and_answers_computable():
    import random

    implicit = {"quad_solve", "deriv_poly3", "integr_poly3"}
    for seed in range(6):
        rng = random.Random(seed)
        for name, rel in RELATIONS.items():
            for _ in range(20):
                params, question = rel.gen(rng)
                assert isinstance(question, str) and question
                call = render_call(name, params)
                assert run_call(call) == rel.solve(*params)
                if name not in implicit:
                    for p in params:
                        assert str(p) in question, f"{name}: arg {p} missing from question"


def test_every_relation_has_a_distractor_template():
    assert set(TEMPLATES) == set(RELATIONS)


def test_distractors_are_plausible_and_harmless():
    import random

    rng = random.Random(0)
    for name, rel in RELATIONS.items():
        for _ in range(25):
            params, question = rel.gen(rng)
            before = rel.solve(*params)
            q2, d = add_distractor(name, params, question, rng)
            assert str(d) in q2
            assert str(d) not in {str(p) for p in params}
            assert rel.solve(*params) == before


def test_quantize_error_is_small():
    rng = np.random.default_rng(0)
    w = rng.normal(0, 0.02, size=(64, 256))
    q, s = quantize(w)
    dq = dequantize(q, s)
    assert np.abs(dq - w).max() < 1e-3
    assert q.dtype == np.int8 and q.max() <= 127 and q.min() >= -127


def test_quantize_rejects_indivisible_columns():
    with pytest.raises(AssertionError):
        quantize(np.zeros((4, 264)))


def test_scale_has_enough_bits():
    rng = np.random.default_rng(1)
    w = rng.normal(0, 0.02, size=(32, 256))
    _, s = quantize(w)
    assert (s > 0).all()
    assert s.min() > (1 << (SCALE_BITS - 16))


def _corpus(n=200):
    import random

    rng = random.Random(5)
    names = sorted(RELATIONS)
    rows = []
    for i in range(n):
        name = names[i % len(names)]
        params, q = RELATIONS[name].gen(rng)
        rows.append({"question": q, "call": render_call(name, params)})
    return rows


def test_tokenizer_roundtrip_and_specials():
    rows = _corpus()
    texts = [f"{r['question']} <sep> {r['call']}" for r in rows]
    tok = train_tokenizer(texts, 512)
    assert tok.size <= 512
    for t in texts:
        assert tok.decode(tok.encode(t)) == t
    for piece in ("CALL", "<sep>", "<bos>", "<eos>", "<pad>"):
        assert tok.encode(piece) == [SPECIALS[piece]]
    for name in RELATIONS:
        assert tok.encode(name) == [SPECIALS[name]]


def test_tokenizer_ids_are_distinct():
    ids = list(SPECIALS.values())
    assert len(ids) == len(set(ids))
    assert min(ids) >= 256


def test_tokenizer_rejects_vocab_below_specials():
    from tokenizer import FIRST_MERGE

    with pytest.raises(ValueError):
        train_tokenizer(["Solve 2x = 4 <sep> CALL quad_solve 0 2 -4"], FIRST_MERGE - 1)


def test_pack_never_straddles_rows():
    from train import pack

    rows = _corpus(60)
    tok = train_tokenizer([f"{r['question']} <sep> {r['call']}" for r in rows], 512)
    seq = 128
    x, y, m = pack(rows, tok, seq)
    sep = SPECIALS["<sep>"]
    for w in range(len(x)):
        window = list(x[w]) + [int(y[w][-1])]
        mask = list(m[w])
        seen_sep = sep in window
        for i, flag in enumerate(mask):
            if flag:
                assert seen_sep, f"window {w} target {i} has no prompt before it"
            if window[i] == sep:
                seen_sep = True


def test_pack_rejects_empty_input():
    from train import pack

    tok = train_tokenizer(["Solve 2x = 4 <sep> CALL quad_solve 0 2 -4"], 300)
    with pytest.raises(SystemExit):
        pack([], tok, 64)


def test_training_reduces_loss(tmp_path):
    from train import main as _  # noqa: F401

    rows = _corpus(400)
    data = tmp_path / "train.jsonl"
    with open(data, "w") as f:
        for r in rows:
            f.write(json.dumps(r) + "\n")

    tok = train_tokenizer([f"{r['question']} <sep> {r['call']}" for r in rows], 512)
    tokp = tmp_path / "tok.json"
    tokp.write_text(json.dumps(tok.to_json()))

    out = tmp_path / "m.pt"
    proc = subprocess.run(
        [sys.executable, "train.py", "--traces", str(data), "--tokenizer", str(tokp),
         "--dim", "32", "--layers", "1", "--heads", "4", "--hidden", "64",
         "--seq", "64", "--batch", "8", "--steps", "30", "--eval-every", "10",
         "--samples", "0", "--out", str(out)],
        cwd=ROOT, capture_output=True, text=True)
    assert proc.returncode == 0, proc.stderr[-2000:]
    import re

    losses = [float(m.group(1)) for line in proc.stdout.splitlines()
              for m in [re.search(r"^\s*step\s+\d+\s+loss\s+([\d.]+)", line)] if m]
    assert len(losses) >= 3
    assert losses[-1] < losses[0], losses
    assert out.exists()


def _build_runtime():
    rt = os.path.join(ROOT, "runtime")
    subprocess.run(["make", "-s"], cwd=rt, check=True, capture_output=True)
    return os.path.join(rt, "infer_host")


def test_c_engine_matches_pytorch(tmp_path):
    from train import Config, Model

    torch.manual_seed(0)
    cfg = Config(256, 2, 8, 256, 512, 32)
    model = Model(cfg)
    ckpt = tmp_path / "m.pt"
    torch.save({"model": model.state_dict(), "config": vars(cfg)}, ckpt)

    ids = [259, 32, 261, 32, 258]
    ref = tmp_path / "ref.npy"
    weights = tmp_path / "w.bin"
    proc = subprocess.run(
        [sys.executable, "export_model.py", "--ckpt", str(ckpt), "--out", str(weights),
         "--ref-ids", ",".join(map(str, ids)), "--ref-out", str(ref)],
        cwd=ROOT, capture_output=True, text=True)
    assert proc.returncode == 0, proc.stderr[-2000:]

    binary = _build_runtime()
    out = subprocess.run([binary, str(weights), *map(str, ids)],
                         capture_output=True, text=True, check=True)
    got = np.array([float(l.split()[2]) for l in out.stdout.splitlines()
                    if l.startswith("logit ")])
    expected = np.load(ref)[-1]
    assert got.shape == expected.shape
    assert np.abs(got - expected).max() < 1e-4
    assert int(got.argmax()) == int(expected.argmax())


def test_export_rejects_bad_vocab_id(tmp_path):
    from train import Config, Model

    cfg = Config(256, 1, 8, 256, 512, 16)
    ckpt = tmp_path / "m.pt"
    torch.save({"model": Model(cfg).state_dict(), "config": vars(cfg)}, ckpt)
    proc = subprocess.run(
        [sys.executable, "export_model.py", "--ckpt", str(ckpt),
         "--out", str(tmp_path / "w.bin"), "--ref-ids", "9999"],
        cwd=ROOT, capture_output=True, text=True)
    assert proc.returncode != 0
