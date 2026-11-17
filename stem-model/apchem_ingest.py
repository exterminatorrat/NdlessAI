"""Turn the College Board AP Chemistry bank into paired question records.

    uv run --with pypdf --with pypdfium2 --with pillow python apchem_ingest.py \
        --corpus "$HOME/Documents/Chemistry/Chemistry CB官方题库" \
        --out corpora/apchem

Writes JSONL and the page cache under `corpora/apchem/`, which is gitignored:
the bank is College Board's copyrighted text and stays on this disk, never in a
commit. `apchem_audit.py` reads what this writes.

Why this is not a `pdftotext` loop. The printable chemistry in these files is
drawn, not typed: equations, tables, Lewis structures, plots and the species in
half the prompts are raster images, so the text layer of a page reads

    Four standard solutions of        were prepared by dissolving   (molar mass   )

and then lists `(A)` through `(D)` with nothing after them. Rendering the page
and reading it with Vision recovers all of it — the equations, the table cells,
the axis labels — at the cost of its own errors, which are systematic: a
subscript 2 comes back as `z`. The two sources are complementary rather than
ranked, because the text layer is exact where it exists (it carries `Na` `2` `O`
as three runs with the 2 at 9.13 pt) and the raster is complete where it does
not. So one merged line list is built per page: OCR for geometry and coverage,
the text layer replacing it line by line wherever the page also typed the run.
Both sources and the merge are recorded per item so a later reader can see which
one a given word came from.

What pairs with what. TB is 题目, SG is 答案. For MCQ the scoring guide is the
same booklet plus a green highlight on the correct option, so the answer key is
geometry, not text: find the highlight band, take the option line under it. For
FRQ the guide restates each prompt and then gives `Part A` … `Part C(ii)` blocks
of point descriptions; those attach to the parts of the same number.

Ids are stable across reruns because they are the bank's own coordinates —
`mcq-u04-013`, `frq-s03-q02-c` — and never a document order or a line number.
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import shutil
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
OCR_SWIFT = os.path.join(HERE, "apchem_ocr.swift")

# 200 DPI reads a 9 pt subscript as a subscript; 250 changed no recognised
# character on a probe page and cost a third more render time.
DPI = 200

# A prompt number sits at the left margin, at 38 pt from the paper edge; the
# option indents are at 67 pt and a floated table at 72. The 46 pt cut between
# them is what `merge_lines` uses to tell "this line starts an item" from "this
# line is inside one".
MARGIN_PT = 46

MCQ_TB = re.compile(r"^tb-unit (\d)-mcq\.pdf$")
MCQ_SG = re.compile(r"^sg-unit (\d)-mcq\.pdf$")
FRQ_TB = re.compile(r"^tb-frq-(\d+)\.pdf$")
FRQ_SG = re.compile(r"^sg-frq-(\d+)\.pdf$")

# Running heads, footers and the exported-app furniture. Dropped before
# segmentation, because a page number shaped like `12.` would otherwise be read
# as question 12.
BOILERPLATE = (
    re.compile(r"^(AP CHEMISTRY|AP Chemistry) (Test Booklet|Scoring Guide)$"),
    re.compile(r"^(Test Booklet|Scoring Guide)$"),
    re.compile(r"^AP CHEMISTRY$"),
    re.compile(r"^AP Chemistry\s+Page \d+ of \d+$"),
    re.compile(r"^Page \d+ of \d+\s+AP Chemistry$"),
    re.compile(r"^Page \d+ of \d+$"),
    re.compile(r"^unit\d+mcq$"),
    re.compile(r"^frq\d+$"),
    re.compile(r"^[©@É€]?\s*College ?Board$"),
    re.compile(r"^AP\s*$"),
)

# The free-response instructions block, repeated before most numbered prompts.
FRQ_PREAMBLE = re.compile(
    r"^(Directions:\s*)?For parts of the free[- ]response question"
    r"|^(and )?(the )?(steps )?involved in arriving at your answers"
    r"|^(Examples and )?equations may be included in your answers")

# A prompt number: `13.` at the start of a line, not the `13.` of `13.5 atm`.
PROMPT = re.compile(r"^(\d{1,3})\.(?!\d)\s*(.*)$")
OPTION = re.compile(r"^\(([A-E])\)\s*(.*)$")
FRQ_PART = re.compile(r"^\(?([a-h])\)?[.)]\s*(.*)$")
FRQ_SUB = re.compile(r"^\((i{1,3}|iv|v|vi{1,3}|ix|x)\)\s*(.*)$")
GUIDE_PART = re.compile(r"^Part ([A-Z])(?:\(([ivx]+)\))?$")
# `1 point is earned for ...`, `Student earns 1/1 point(s)`, and the exported
# `0 1` score marks are the guide's own furniture, not chemistry.
# Only the exported app's bookkeeping. `1 point is earned for the correct
# calculation of the energy` is the rubric itself and dropping it left the
# simplest parts with no scoring text at all.
GUIDE_FURNITURE = re.compile(
    r"^(Student earns \d+/\d+ point|0 1$|"
    r"Select a point value to view scoring criteria|"
    r"[•*]\s*(CollegeBoard|AP Chemistry)\b|AP Chemistry$|CollegeBoard$)")


# Vision reads a Cyrillic `А` for a Latin `A` in small caps, which is enough to
# walk a running head past the boilerplate list and into a question.
HOMOGLYPH = str.maketrans("АВЕКМНОРСТХ", "ABEKMHOPCTX")


def boilerplate(text):
    return any(rx.match(text.translate(HOMOGLYPH)) for rx in BOILERPLATE)


def clean(text):
    return re.sub(r"\s+", " ", text).strip()


# --------------------------------------------------------------------------
# Page rendering and reading


def build_ocr(cache):
    """Compile the Vision reader once per source revision."""

    target = os.path.join(cache, "apchem_ocr")
    stamp = hashlib.sha256(open(OCR_SWIFT, "rb").read()).hexdigest()[:12]
    marker = target + ".src"
    if os.path.exists(target) and os.path.exists(marker):
        if open(marker).read().strip() == stamp:
            return target
    out = target + ".tmp"
    subprocess.run(["swiftc", "-O", OCR_SWIFT, "-o", out], check=True)
    os.replace(out, target)
    open(marker, "w").write(stamp)
    return target


def typed_lines(page):
    """The PDF text layer as lines with pixel boxes, subscripts glued back on.

    The layer stores `Na`, `2`, `O` as three runs, the 2 at a smaller size and a
    lower baseline, which is why a naive extractor either loses the subscript or
    detaches it. Runs are grouped by baseline and ordered by x, which is what
    makes `Na2O` come back out.
    """

    scale = DPI / 72.0
    top = page.mediabox.top
    runs = []

    def visit(text, cm, tm, font, size):
        if not text.strip():
            return
        x = tm[4] * cm[0] + tm[5] * cm[2] + cm[4]
        y = tm[4] * cm[1] + tm[5] * cm[3] + cm[5]
        runs.append((y, x, size or 10.0, text))

    page.extract_text(visitor_text=visit)
    runs.sort(key=lambda r: (-r[0], r[1]))

    lines = []
    for y, x, size, text in runs:
        if lines and abs(lines[-1][0] - y) < max(2.5, size * 0.35):
            lines[-1][1].append((x, text))
            lines[-1][0] = (lines[-1][0] * (len(lines[-1][1]) - 1) + y) / len(lines[-1][1])
        else:
            lines.append([y, [(x, text)]])
    out = []
    for y, chunks in lines:
        chunks.sort(key=lambda c: c[0])
        text = clean("".join(c[1] for c in chunks))
        if not text:
            continue
        out.append({
            "t": text,
            "x": int(chunks[0][0] * scale),
            "y": int((top - y) * scale),
            "w": 0,
            "h": 0,
            "src": "text",
        })
    return out


def _same(first, second):
    """Two readings of one line, from OCR twice or from OCR and the layer."""

    if abs(first["y"] - second["y"]) >= 14:
        return False
    if first["t"][:20] == second["t"][:20]:
        return True
    shorter, longer = sorted((first["t"], second["t"]), key=len)
    return len(shorter) > 12 and shorter in longer


def merge_lines(ocr, typed):
    """OCR geometry, typed text, per line.

    A typed run wins when the page also typed that line, because the layer is
    exact where it exists; OCR keeps every line the layer does not have and
    supplies the box geometry for both. What is left over is superscripts and
    subscripts — the layer sets an exponent at its own baseline, so `1.2 x 10`
    and its `23` arrive as two lines and the merge used to drop the second into
    the stem — and those attach to whichever merged line contains them.
    """

    used = set()
    merged = []
    for line in ocr:
        centre = line["y"] + 0.75 * max(line["h"], 8)
        best, best_dy = None, None
        for index, other in enumerate(typed):
            if index in used:
                continue
            dy = abs(other["y"] - centre)
            if dy <= 0.7 * max(line["h"], 8) + 2 and other["x"] + max(other["w"], 0) > line["x"] - 24:
                if best_dy is None or dy < best_dy:
                    best, best_dy = index, dy
        entry = dict(line, t=typed[best]["t"], src="text") if best is not None else dict(line, src="ocr")
        if best is not None:
            used.add(best)
        if any(_same(entry, other) for other in merged):
            continue
        merged.append(entry)

    for index, other in enumerate(typed):
        if index in used:
            continue
        if not re.search(r"[0-9]|[A-Za-z]{2}", other["t"]):
            continue
        if any(_same(other, line) for line in merged):
            continue
        host = None
        for line in merged:
            height = max(line["h"], 10)
            if (line["y"] - 8 <= other["y"] <= line["y"] + height + 8
                    and line["x"] - 8 <= other["x"] <= line["x"] + max(line["w"], 0) + 8):
                host = line
                break
        if host is not None:
            if other["t"] not in host["t"]:
                host["t"] = host["t"].rstrip() + other["t"]
            continue
        merged.append(dict(other, src="text"))

    # A reference table floats at the top of the item that uses it, so its
    # header row sits a few pixels above the prompt number to its left and a
    # plain y-x sort reads the table into the previous item and pushes the
    # prompt's own sentence into the next one. A prompt number in the left
    # margin is hoisted a line so it leads its own block.
    def order(line):
        hoist = PROMPT.match(line["t"]) and line["x"] < MARGIN_PT * DPI / 72
        return (line["y"] - (22 if hoist else 0), line["x"])

    merged.sort(key=order)
    return merged


def answer_bands(image):
    """Rows covered by the scoring guide's green highlight, as (top, bottom).

    The exported MCQ scoring guide marks the correct option by drawing a pale
    green band behind it — (230,255,230) with the text at (58,145,63). There is
    no text and no annotation that says which one, so the key is geometry.
    """

    import numpy

    frame = numpy.asarray(image.convert("RGB")).astype("int16")
    red, green, blue = frame[:, :, 0], frame[:, :, 1], frame[:, :, 2]
    mask = (green > red + 8) & (green > blue + 8)
    rows = numpy.nonzero(mask.any(axis=1))[0]
    if not len(rows):
        return []
    bands, start, last = [], rows[0], rows[0]
    for row in rows[1:]:
        if row - last > 4:
            bands.append((int(start), int(last)))
            start = row
        last = row
    bands.append((int(start), int(last)))
    wide = int(mask.shape[1] * 0.25)
    # The highlight is about 26 pt tall, and the bounds are in points so that a
    # change of DPI cannot quietly empty the answer key the way 200 DPI did to
    # a 60 px ceiling.
    shortest = 8 * DPI / 72
    tallest = 60 * DPI / 72
    out = []
    for top, bottom in bands:
        if bottom - top < shortest or bottom - top > tallest:
            continue
        if mask[top:bottom + 1].sum(axis=1).max() < wide:
            continue
        out.append((top, bottom))
    return out


def read_pdf(path, cache, ocr=True):
    """Every page of one PDF, cached by file hash so a rerun is cheap.

    Rendering and OCR are separated on purpose: rendering is 39 pages a second
    and OCR is not, and one recogniser fed a whole file behaves where a process
    per page wedged the Vision service after a few hundred pages.
    """

    # The key carries the mode: a text-only pass and an OCR pass read different
    # amounts of the same page and must not answer for each other.
    digest = hashlib.sha256(open(path, "rb").read()).hexdigest()[:16]
    digest += "-ocr" if ocr else "-txt"
    target = os.path.join(cache, os.path.basename(digest + ".json"))
    if os.path.exists(target):
        return json.load(open(target))

    import pypdf
    import pypdfium2

    document = pypdfium2.PdfDocument(path)
    reader = pypdf.PdfReader(path)
    count = len(document)
    ocr_binary = build_ocr(cache)

    workspace = tempfile.mkdtemp(prefix="apchem-")
    try:
        shots = []
        for index in range(count):
            image = document[index].render(scale=DPI / 72.0).to_pil()
            png = os.path.join(workspace, f"{index:04d}.png")
            image.save(png)
            shots.append((index, png, image))
        recognised = ocr_pages([png for _, png, _ in shots], ocr_binary, cache) if ocr else {}
        pages = []
        for index, png, image in shots:
            payload = recognised.get(png, {})
            pages.append({
                "w": payload.get("w", 0),
                "h": payload.get("h", 0),
                "ocr": payload.get("lines", []),
                "typed": typed_lines(reader.pages[index]),
                "bands": answer_bands(image),
                "ocr_failed": ocr and png not in recognised,
            })
        stragglers = sum(1 for page in pages if page["ocr_failed"])
        if stragglers:
            print(f"  {os.path.basename(path)}: {stragglers} pages without OCR, pausing",
                  flush=True)
            time.sleep(45)
    finally:
        shutil.rmtree(workspace, ignore_errors=True)
        document.close()

    record = {
        "file": os.path.basename(path),
        "sha256": hashlib.sha256(open(path, "rb").read()).hexdigest(),
        "pages": pages,
    }
    json.dump(record, open(target + ".tmp", "w"))
    os.replace(target + ".tmp", target)
    return record


def ocr_pages(pngs, binary, cache, stall=45):
    """Run one recogniser over a list of pages, surviving a stall.

    Vision stops answering rather than returning an error when it is pushed; the
    process is killed once it has been silent for `stall` seconds and the pages
    it did not reach are retried in a fresh process. A page that defeats the
    second attempt keeps its text layer and is recorded as failed, never filled
    with a guess.
    """

    pending = list(pngs)
    found, failures = {}, set()
    for attempt in range(4):
        if not pending:
            break
        if attempt:
            time.sleep((25, 60, 120)[attempt - 1])
            # A refusal is often transient, so a marked page is offered again on
            # the next sweep before it is given up on.
            pending = sorted(set(pending) | failures)
            failures.clear()
        before = len(found)
        # Once the Neural Engine has refused a request it refuses every one until
        # the machine is rebooted, so the verdict is cached rather than paid for
        # on each of the 38 files.
        flags = ["--fast"] if os.path.exists(os.path.join(cache, "fast-only")) else []
        process = subprocess.Popen([binary, *flags, "-"], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, bufsize=1)
        try:
            process.stdin.write("\n".join(pending) + "\n")
            process.stdin.close()
            deadline = time.time() + stall * len(pending)
            while True:
                line = process.stdout.readline()
                if not line:
                    break
                payload = json.loads(line)
                if payload.get("failed"):
                    failures.add(payload["path"])
                    pending = [p for p in pending if p != payload["path"]]
                else:
                    found[payload["path"]] = payload
                    pending = [p for p in pending if p not in found]
                deadline = time.time() + stall * max(len(pending), 1)
                if time.time() > deadline:
                    break
                if not pending:
                    break
        finally:
            if process.poll() is None:
                process.kill()
            process.wait()
        if attempt and len(found) == before:
            # Nothing came back at all: the recogniser is down for a while, and
            # grinding page by page would burn the whole run on one file.
            break
        problem = process.stderr.read() if process.stderr else ""
        if "accurate recognition unavailable" in problem:
            open(os.path.join(cache, "fast-only"), "w").write(problem[:400])
        if process.returncode not in (0, None) and not pending:
            break
    return found


# --------------------------------------------------------------------------
# Segmentation


class Line:
    __slots__ = ("t", "x", "y", "page", "src")

    def __init__(self, raw, page):
        self.t = raw["t"]
        self.x = raw["x"]
        self.y = raw["y"]
        self.src = raw.get("src", "ocr")
        self.page = page

    def __repr__(self):
        return f"{self.page}:{self.t[:40]!r}"


def flow(record):
    """Every non-furniture line of a booklet, in reading order."""

    out = []
    for index, page in enumerate(record["pages"]):
        for raw in merge_lines(page["ocr"], page["typed"]):
            text = clean(raw["t"])
            if not text or boilerplate(text):
                continue
            out.append(Line(dict(raw, t=text), index))
    return out


def segment_mcq(lines):
    """Numbered stems with `(A)`-`(D)` options, and the stimulus that owns them.

    The booklet groups two to four questions under one experiment description,
    so text between an item's last option and the next number belongs to that
    next item's set, not to the item before it.
    """

    items, current, context = [], None, []
    expected = 1
    margin = None

    for line in lines:
        prompt = PROMPT.match(line.t)
        if prompt and expected <= int(prompt.group(1)) <= expected + 2:
            if current:
                items.append(current)
            margin = line.x
            current = {
                "number": int(prompt.group(1)),
                "stimulus": " ".join(context).strip(),
                "shared_stimulus": not context and bool(items),
                "stem": [prompt.group(2)],
                "options": {},
                "option_page": line.page,
                "pages": {line.page},
                "src": {line.src},
            }
            expected = current["number"] + 1
            context = []
            continue
        if current is None:
            if line.t:
                context.append(line.t)
            continue

        option = OPTION.match(line.t)
        if option and option.group(1) not in current["options"]:
            current["options"][option.group(1)] = [option.group(2)]
            current["option_page"] = line.page
            current["pages"].add(line.page)
            current["src"].add(line.src)
            continue

        wide = margin is not None and line.x > margin + 12
        if current["options"] and (not wide or line.page != current["option_page"]):
            context.append(line.t)
            continue
        current["pages"].add(line.page)
        current["src"].add(line.src)
        if current["options"]:
            last = max(option for option in current["options"])
            current["options"][last].append(line.t)
        else:
            current["stem"].append(line.t)

    if current:
        items.append(current)
    return items


def segment_frq(lines):
    """Numbered free-response prompts, split into lettered parts and sub-parts."""

    items, current, part, sub = [], None, None, None
    expected = 1
    for line in lines:
        if FRQ_PREAMBLE.match(line.t):
            continue

        prompt = PROMPT.match(line.t)
        if prompt and expected <= int(prompt.group(1)) <= expected + 2:
            if current:
                if part:
                    current["parts"].append(part)
                items.append(current)
            rest = prompt.group(2)
            if FRQ_PREAMBLE.match(rest):
                rest = ""
            current = {
                "number": int(prompt.group(1)),
                "stem": [rest],
                "body": [rest],
                "parts": [],
                "pages": {line.page},
            }
            expected = current["number"] + 1
            part = sub = None
            continue
        if current is None:
            continue
        current["pages"].add(line.page)
        current["body"].append(line.t)

        letter = FRQ_PART.match(line.t)
        if letter and (part is None or ord(letter.group(1)) == ord(part["letter"]) + 1):
            if part:
                current["parts"].append(part)
            part = {"letter": letter.group(1), "text": [letter.group(2)], "subs": []}
            sub = None
            continue

        inner = FRQ_SUB.match(line.t)
        if inner and part:
            sub = {"label": inner.group(1), "text": [inner.group(2)]}
            part["subs"].append(sub)
            continue

        if sub is not None:
            sub["text"].append(line.t)
        elif part is not None:
            part["text"].append(line.t)
        else:
            current["stem"].append(line.t)

    if current:
        if part:
            current["parts"].append(part)
        items.append(current)
    return items


def guide_blocks(lines):
    """`Part A` … `Part C(ii)` blocks from a free-response scoring guide, in order.

    The guide restates the prompt first and then explains the points, so its own
    prompt text is skipped: the record takes its questions from the question
    booklet, which has no scoring furniture in it.

    Blocks are grouped by the part letters resetting, not by the prompt number.
    The number is drawn on the page in most of these sets, so the text layer
    finds only the first one or two, and keying on it handed prompt 1's rubric to
    whatever prompt happened to have a readable number. The guide lists its
    prompts in the booklet's order, so a part `A` after any other part starts
    the next prompt, and the Nth group belongs to the Nth prompt.
    """

    blocks, index, key, buffer = [], 1, None, []
    previous = None

    def flush():
        if key is not None and " ".join(buffer).strip():
            blocks.append((index, key, " ".join(buffer).strip()))

    for line in lines:
        heading = GUIDE_PART.match(line.t)
        if heading:
            flush()
            letter = heading.group(1).lower()
            if letter == "a" and previous is not None:
                index += 1
            previous = letter
            key = letter + (heading.group(2) or "")
            buffer = []
            continue
        if key is None or GUIDE_FURNITURE.match(line.t):
            continue
        buffer.append(line.t)
    flush()
    return blocks


def answer_key(record):
    """Option letter under each green band, per item number.

    Both issues come out of the same export, so page N of the guide holds the
    items page N of the booklet holds, but the pairing is made on the number
    found on the page rather than on that assumption.
    """

    keys = {}
    latest = 0
    for page in record["pages"]:
        lines = merge_lines(page["ocr"], page["typed"])
        numbers = [(int(m.group(1)), line["y"]) for line in lines
                   for m in [PROMPT.match(clean(line["t"]))] if m]
        if not page["bands"]:
            if numbers:
                latest = max(latest, max(n for n, _ in numbers))
            continue
        for top, bottom in page["bands"]:
            centre = (top + bottom) / 2.0
            letter = None
            for line in lines:
                option = OPTION.match(clean(line["t"]))
                if not option:
                    continue
                if top - 2 <= line["y"] + line["h"] / 2.0 <= bottom + 2:
                    letter = option.group(1)
                    break
            if letter is None:
                continue
            # An item whose stem begins on the previous page has no number above
            # its highlight, so the count carries over rather than skipping it.
            above = [n for n, y in numbers if y <= centre]
            keys[max(above) if above else latest] = letter
        if numbers:
            latest = max(latest, max(n for n, _ in numbers))
    return keys


# --------------------------------------------------------------------------
# Assembly


def build_mcq(corpus, cache, ocr=True):
    tb_dir = os.path.join(corpus, "ap chemistry mcq")
    records = []
    stats = []
    for name in sorted(os.listdir(tb_dir)):
        question_book = MCQ_TB.match(name)
        guide_book = MCQ_SG.match(name)
        if question_book:
            unit = int(question_book.group(1))
            record = read_pdf(os.path.join(tb_dir, name), cache, ocr)
            keys = {}
            guide_record = None
            guide_name = f"sg-unit {unit}-mcq.pdf"
            if os.path.exists(os.path.join(tb_dir, guide_name)):
                guide_record = read_pdf(os.path.join(tb_dir, guide_name), cache, ocr)
            items = segment_mcq(flow(record))
            if guide_record:
                keys = answer_key(guide_record)
            for item in items:
                options = {}
                for letter, chunks in item["options"].items():
                    text = clean(" ".join(chunks))
                    options[letter] = text
                records.append({
                    "id": f"mcq-u{unit:02d}-{item['number']:03d}",
                    "kind": "mcq",
                    "unit": unit,
                    "number": item["number"],
                    "source": f"ap chemistry mcq/{name}",
                    "guide_source": f"ap chemistry mcq/{guide_name}" if guide_record else None,
                    "pages": sorted(item["pages"]),
                    "stimulus": item["stimulus"],
                    "shared_stimulus": item["shared_stimulus"],
                    "stem": clean(" ".join(item["stem"])),
                    "options": options,
                    "answer": keys.get(item["number"]),
                    "sources": sorted(item["src"]),
                })
            seen = {item["number"] for item in items}
            stats.append((unit, len(items), max(item["number"] for item in items) if items else 0,
                          len(seen & set(keys))))
        elif guide_book:
            continue
    return records, stats


def build_frq(corpus, cache, ocr=True):
    tb_dir = os.path.join(corpus, "ap chemistry frq")
    records, stats = [], []
    for name in sorted(os.listdir(tb_dir), key=lambda n: (len(n), n)):
        question_book = FRQ_TB.match(name)
        if not question_book:
            continue
        number = int(question_book.group(1))
        record = read_pdf(os.path.join(tb_dir, name), cache, ocr)
        guide_record = None
        guide_name = f"sg-frq-{number}.pdf"
        if os.path.exists(os.path.join(tb_dir, guide_name)):
            guide_record = read_pdf(os.path.join(tb_dir, guide_name), cache, ocr)
        blocks = guide_blocks(flow(guide_record)) if guide_record else []
        guides = {}
        for index, key, text in blocks:
            guides.setdefault(index, {})[key] = text
        items = segment_frq(flow(record))
        parts = 0
        for ordinal, item in enumerate(items, start=1):
            rubric = guides.get(ordinal, {})
            out_parts = []
            for part in item["parts"]:
                parts += 1
                key = part["letter"]
                guide = rubric.get(key, "")
                subs = []
                for sub in part["subs"]:
                    subs.append({
                        "label": sub["label"],
                        "text": clean(" ".join(sub["text"])),
                        "guide": rubric.get(key + sub["label"], ""),
                    })
                out_parts.append({
                    "id": f"frq-s{number:02d}-q{item['number']:02d}-{key}",
                    "letter": key,
                    "text": clean(" ".join(part["text"])),
                    "guide": guide,
                    "subs": subs,
                })
            records.append({
                "id": f"frq-s{number:02d}-q{item['number']:02d}",
                "kind": "frq",
                "set": number,
                "number": item["number"],
                "source": f"ap chemistry frq/{name}",
                "guide_source": f"ap chemistry frq/{guide_name}" if guide_record else None,
                "pages": sorted(item["pages"]),
                "stem": clean(" ".join(item["stem"])),
                "parts": out_parts,
                "guide_parts": sorted(rubric),
            })
        stats.append((number, len(items), parts, len(blocks)))
    return records, stats


def write_jsonl(path, records):
    with open(path, "w") as handle:
        for record in records:
            handle.write(json.dumps(record, ensure_ascii=False) + "\n")


def main():
    argument = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    argument.add_argument("--corpus", required=True, help="the bank's top directory")
    argument.add_argument("--out", default=os.path.join(HERE, "corpora", "apchem"))
    argument.add_argument("--no-ocr", action="store_true",
                          help="text layer only; the answer highlights still work")
    argument.add_argument("--retry-failed", action="store_true",
                          help="drop cached files that lost pages to the recogniser")
    args = argument.parse_args()

    cache = os.path.join(args.out, "pages")
    os.makedirs(cache, exist_ok=True)

    if args.retry_failed:
        for entry in sorted(os.listdir(cache)):
            if not entry.endswith(".json"):
                continue
            path = os.path.join(cache, entry)
            record = json.load(open(path))
            if any(page.get("ocr_failed") for page in record.get("pages", [])):
                os.unlink(path)
                print(f"  retrying {record.get('file')}", flush=True)

    ocr = not args.no_ocr
    mcq, mcq_stats = build_mcq(args.corpus, cache, ocr)
    frq, frq_stats = build_frq(args.corpus, cache, ocr)

    write_jsonl(os.path.join(args.out, "mcq.jsonl"), mcq)
    write_jsonl(os.path.join(args.out, "frq.jsonl"), frq)

    provenance = {
        "corpus": os.path.abspath(args.corpus),
        "dpi": DPI,
        "files": {},
        "counts": {"mcq": len(mcq), "frq": len(frq)},
    }
    for root, _, names in os.walk(args.corpus):
        for name in sorted(names):
            if not name.lower().endswith(".pdf"):
                continue
            path = os.path.join(root, name)
            provenance["files"][os.path.relpath(path, args.corpus)] = hashlib.sha256(
                open(path, "rb").read()).hexdigest()
    json.dump(provenance, open(os.path.join(args.out, "provenance.json"), "w"), indent=1)

    print(f"mcq: {len(mcq)} items")
    for unit, found, highest, keyed in mcq_stats:
        print(f"  unit {unit}: {found} items, highest number {highest}, "
              f"{keyed} with an answer key")
    print(f"frq: {len(frq)} prompts")
    for number, found, parts, guides in frq_stats:
        print(f"  set {number}: {found} prompts, {parts} parts, {guides} guide blocks")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
