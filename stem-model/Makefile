PY := uv run --with torch --with numpy --with sympy
DATA := data

all: data train

data:
	$(PY) python make_dataset.py --out $(DATA)

train:
	$(PY) python train.py --traces $(DATA)/train.jsonl \
		--tokenizer $(DATA)/tokenizer.json --seq 128 --batch 32 \
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
