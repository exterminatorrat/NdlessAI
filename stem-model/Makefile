PY := uv run --with torch --with numpy --with sympy
DATA := data

all: data train

# The corpus's train split is mixed in because that is what the corpus's split
# exists for, and because the published accuracy numbers were produced with it.
# Without it, two of every five hand-written phrasings per relation are never
# seen and the paraphrase gate reads about 39 points lower.
data:
	$(PY) python make_dataset.py --out $(DATA) --paraphrases 'fixtures/paraphrase_*.jsonl'

# --seq 384 is the reading window, and it is one number in three places: here,
# the packer's cap (seq + 1, in train.py) and the device's own SEQ in
# device/main.c. A window raised in the trainer alone produces a model whose
# questions the device refuses at the tail, with no error anywhere.
#
# The corpus is what bounds it at 384: the longest question plus call in the
# r9 corpus is 179 tokens, so the packer drops nothing at either width and the
# widening buys no extra training rows - only room for long questions at
# inference. At 192 the wall was the width, not the corpus.
train:
	$(PY) python train.py --traces $(DATA)/train.jsonl \
		--tokenizer $(DATA)/tokenizer.json --seq 384 --batch 32 \
		--steps 12000 --eval-every 500 --out model.pt

export:
	$(PY) python export_model.py --ckpt model.pt --out weights.bin \
		--ref-ids 259,32,261,32,258 --ref-out ref.npy
	$(PY) python export_tokenizer.py --tokenizer $(DATA)/tokenizer.json

host:
	$(MAKE) -C runtime

eval: export host
	$(PY) python check_tokenizer_sync.py $(DATA)/tokenizer.json
	$(PY) python evaluate.py

eval-device:
	$(MAKE) -C runtime
	$(PY) python check_tokenizer_sync.py $(DATA)/tokenizer.json
	$(PY) python evaluate_device.py

test:
	uv run --with pytest --with torch --with numpy --with sympy \
		python -m pytest tests -q

clean:
	rm -rf $(DATA) model.pt weights.bin ref.npy __pycache__ tests/__pycache__
	$(MAKE) -C runtime clean

.PHONY: all data train export host eval eval-device test clean
