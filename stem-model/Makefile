PY := uv run --with torch --with numpy --with sympy
DATA := data

all: data train

data:
	$(PY) python make_dataset.py --out $(DATA)

# --seq 192, not 128: the longest question plus call is 178 tokens, and the
# packer drops anything longer. At 128 that is 0.46% of rows, and the rows it
# drops are the long ones - which is what a five- or six-argument relation looks
# like. The device prompt buffer is 256, so nothing here is untrainable on
# device; this is only the training window.
train:
	$(PY) python train.py --traces $(DATA)/train.jsonl \
		--tokenizer $(DATA)/tokenizer.json --seq 192 --batch 32 \
		--steps 6000 --eval-every 250 --out model.pt

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
