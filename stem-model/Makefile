PY := uv run --with torch --with numpy --with sympy
DATA := data

all: data train

data:
	$(PY) python make_dataset.py --out $(DATA)

train:
	$(PY) python train.py --traces $(DATA)/train.jsonl \
		--tokenizer $(DATA)/tokenizer.json --out model.pt

export:
	$(PY) python export_model.py --ckpt model.pt --out weights.bin \
		--ref-ids 259,32,261,32,258 --ref-out ref.npy

host:
	$(MAKE) -C runtime

test:
	uv run --with pytest --with torch --with numpy --with sympy \
		python -m pytest tests -q

clean:
	rm -rf $(DATA) model.pt weights.bin ref.npy __pycache__ tests/__pycache__
	$(MAKE) -C runtime clean

.PHONY: all data train export host test clean
